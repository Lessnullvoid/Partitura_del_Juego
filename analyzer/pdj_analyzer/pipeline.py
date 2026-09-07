"""Replaceable offline analysis adapters and a stdlib synthetic backend."""

from __future__ import annotations

import hashlib
import json
import math
import struct
import subprocess
from pathlib import Path
from typing import Any, Dict, List, Optional, Protocol, Tuple

from .anatomy import JOINT_NAMES, anatomical_target, assign_region
from .pdjv import (
    FrameRecord,
    PdjvPackage,
    PlayerRecord,
    SpatialBlock,
    default_manifest,
    export_package,
)


class Detector(Protocol):
    def detect(self, frame_index: int, timestamp: float) -> List[Dict[str, Any]]:
        ...


def source_id_for(path: Path, width: int, height: int, fps: float, frames: int) -> str:
    digest = hashlib.sha1(f"{path.name}|{width}x{height}|{fps}|{frames}".encode()).hexdigest()[:12]
    return f"{path.stem}_{digest}"


def inspect_clip(path: Path) -> Dict[str, Any]:
    """Read container metadata with ffprobe when available."""
    info = {
        "path": str(path),
        "width": 1920,
        "height": 1080,
        "fps": 30.0,
        "frameCount": 90,
        "probe": "default",
    }
    if not path.exists():
        info["probe"] = "missing"
        return info
    try:
        proc = subprocess.run(
            [
                "ffprobe",
                "-v",
                "error",
                "-select_streams",
                "v:0",
                "-show_entries",
                "stream=width,height,r_frame_rate,nb_frames,duration",
                "-of",
                "json",
                str(path),
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=20,
        )
        if proc.returncode == 0 and proc.stdout:
            data = json.loads(proc.stdout)
            stream = (data.get("streams") or [{}])[0]
            info["width"] = int(stream.get("width") or 1920)
            info["height"] = int(stream.get("height") or 1080)
            rate = stream.get("r_frame_rate") or "30/1"
            num, den = rate.split("/")
            info["fps"] = float(num) / max(float(den), 1.0)
            if stream.get("nb_frames") and str(stream["nb_frames"]).isdigit():
                info["frameCount"] = int(stream["nb_frames"])
            elif stream.get("duration"):
                info["frameCount"] = max(1, int(float(stream["duration"]) * info["fps"]))
            info["probe"] = "ffprobe"
    except (FileNotFoundError, subprocess.TimeoutExpired, json.JSONDecodeError, ValueError):
        info["probe"] = "fallback"
    return info


def _human_silhouette_mask(width: int, height: int, confidence: float = 0.94) -> SpatialBlock:
    """Procedural full-body silhouette: head, torso, arms, legs at >=96x192."""
    rows = bytearray(width * height)
    cx = (width - 1) * 0.5
    head_cy = height * 0.11
    head_rx, head_ry = width * 0.11, height * 0.075
    torso_top, torso_bot = height * 0.17, height * 0.52
    torso_rx = width * 0.17
    hip_y = height * 0.54
    leg_rx = width * 0.09
    arm_rx = width * 0.07
    arm_top, arm_bot = height * 0.20, height * 0.48

    def ellipse(x: float, y: float, ex: float, ey: float, rx: float, ry: float) -> bool:
        u = (x - ex) / max(rx, 1e-5)
        v = (y - ey) / max(ry, 1e-5)
        return u * u + v * v <= 1.0

    def capsule(x: float, y: float, x0: float, y0: float, x1: float, y1: float, r: float) -> bool:
        dx, dy = x1 - x0, y1 - y0
        len2 = dx * dx + dy * dy
        if len2 < 1e-6:
            return math.hypot(x - x0, y - y0) <= r
        t = max(0.0, min(1.0, ((x - x0) * dx + (y - y0) * dy) / len2))
        px, py = x0 + t * dx, y0 + t * dy
        return math.hypot(x - px, y - py) <= r

    val = int(255 * confidence)
    for y in range(height):
        for x in range(width):
            inside = False
            if ellipse(x, y, cx, head_cy, head_rx, head_ry):
                inside = True
            elif abs(x - cx) <= torso_rx and torso_top <= y <= torso_bot:
                inside = True
            elif capsule(x, y, cx - width * 0.24, arm_top, cx - width * 0.26, arm_bot, arm_rx):
                inside = True
            elif capsule(x, y, cx + width * 0.24, arm_top, cx + width * 0.26, arm_bot, arm_rx):
                inside = True
            elif capsule(x, y, cx - width * 0.08, hip_y, cx - width * 0.10, height * 0.96, leg_rx):
                inside = True
            elif capsule(x, y, cx + width * 0.08, hip_y, cx + width * 0.10, height * 0.96, leg_rx):
                inside = True
            rows[y * width + x] = val if inside else 0
    return SpatialBlock(width, height, bytes(rows))


def _global_depth_crop(width: int, height: int, centroid_y: float, centroid_x: float = 0.5) -> SpatialBlock:
    """Per-pixel depth gradient from global field + centroid."""
    base = 0.35 + centroid_y * 0.4
    out = bytearray()
    for y in range(height):
        for x in range(width):
            nx = (x / max(width - 1, 1)) - centroid_x
            ny = (y / max(height - 1, 1)) - centroid_y
            radial = math.sqrt(nx * nx + ny * ny)
            local = base + ny * 0.10 + radial * 0.06 + (y / max(height - 1, 1)) * 0.05
            value = max(1, min(65535, int(local * 65535)))
            out += struct.pack("<H", value)
    return SpatialBlock(width, height, bytes(out))


def _motion_grid(width: int, height: int, vx: float, vy: float) -> SpatialBlock:
    out = bytearray()
    for _ in range(height):
        for _ in range(width):
            out += struct.pack("<hh", int(max(-32767, min(32767, vx * 4000))), int(max(-32767, min(32767, vy * 4000))))
    return SpatialBlock(width, height, bytes(out))


def _pose(bbox: Tuple[float, float, float, float], phase: float) -> List[Tuple[float, float, float, float]]:
    x, y, w, h = bbox
    cx = x + w * 0.5
    stride = math.sin(phase) * w * 0.1

    def jx(side: float) -> float:
        return cx + side * w

    def jy(from_top: float) -> float:
        return y + from_top * h

    def jz(from_top: float) -> float:
        return 0.40 + (0.5 - from_top) * 0.16

    layout = {
        "nose": (0.0, 0.07),
        "left_eye": (-0.07, 0.09),
        "right_eye": (0.07, 0.09),
        "left_ear": (-0.12, 0.11),
        "right_ear": (0.12, 0.11),
        "left_shoulder": (-0.24, 0.20),
        "right_shoulder": (0.24, 0.20),
        "left_elbow": (-0.34, 0.38),
        "right_elbow": (0.34, 0.38),
        "left_wrist": (-0.30, 0.55),
        "right_wrist": (0.30, 0.55),
        "left_hip": (-0.13, 0.50),
        "right_hip": (0.13, 0.50),
        "left_knee": (-0.14 + stride / max(w, 1e-5), 0.74),
        "right_knee": (0.14 - stride / max(w, 1e-5), 0.74),
        "left_ankle": (-0.12 + stride / max(w, 1e-5), 0.96),
        "right_ankle": (0.12 - stride / max(w, 1e-5), 0.96),
    }
    joints = []
    for name in JOINT_NAMES:
        sx, fy = layout[name]
        joints.append((jx(sx), jy(fy), jz(fy), 0.94))
    return joints


def apply_homography(x: float, y: float, h: List[float]) -> Tuple[float, float]:
    denom = h[6] * x + h[7] * y + h[8]
    if abs(denom) < 1e-8:
        return 0.0, 0.0
    return (h[0] * x + h[1] * y + h[2]) / denom, (h[3] * x + h[4] * y + h[5]) / denom


def synthesize_package(
    source_id: str,
    width: int,
    height: int,
    fps: float,
    frame_count: int,
    package_id: str = "golden-v0",
    occlude_from: Optional[int] = None,
) -> PdjvPackage:
    frames: List[FrameRecord] = []
    events = []
    # Homografía de campo de tipo identidad para el plano general sintético.
    homography = [1, 0, 0, 0, 1, 0, 0, 0, 1]
    for i in range(frame_count):
        t = i / fps
        players = []
        paths = (
            (7, 0.27 + 0.18 * math.sin(t * 1.2), 0.58, 0.16, 0.64),
            (11, 0.70 - 0.14 * math.cos(t * 1.05), 0.56, 0.15, 0.60),
        )
        for tracking, cx, cy, bw, bh in paths:
            observed = not (tracking == 11 and occlude_from is not None and i >= occlude_from and i < occlude_from + 6)
            vx = (0.18 * 1.2 * math.cos(t * 1.2)) if tracking == 7 else (0.14 * 1.05 * math.sin(t * 1.05))
            bbox = (cx - bw * 0.5, cy - bh * 0.5, bw, bh)
            joints = _pose(bbox, t * 8.0)
            ankles = [joints[15], joints[16]]
            ground = (
                0.5 * (ankles[0][0] + ankles[1][0]),
                max(ankles[0][1], ankles[1][1]),
            )
            field = apply_homography(ground[0], ground[1], homography)
            mw, mh = 96, 192
            players.append(
                PlayerRecord(
                    tracking_id=tracking,
                    observed=observed,
                    confidence=0.94 if observed else 0.2,
                    image_centroid=(cx, cy),
                    velocity=(vx, 0.0),
                    acceleration=(-0.15 * math.sin(t * 1.7), 0.0) if tracking == 7 else (0.0, 0.0),
                    bounding_box=bbox,
                    field_position=field,
                    ground_contact=ground,
                    estimated_scale=bh,
                    field_confidence=0.8,
                    joints=joints,
                    mask=_human_silhouette_mask(mw, mh),
                    depth=_global_depth_crop(mw, mh, cy, cx),
                    motion=_motion_grid(6, 10, vx, 0.0),
                )
            )
        if i == 20:
            events.append(
                {
                    "id": 0,
                    "timestamp": t,
                    "type": "acceleration_peak",
                    "x": players[0].image_centroid[0],
                    "y": players[0].image_centroid[1],
                    "confidence": 0.7,
                    "trackingIds": [7],
                }
            )
        cx = sum(p.image_centroid[0] for p in players) / len(players)
        cy = sum(p.image_centroid[1] for p in players) / len(players)
        frames.append(
            FrameRecord(
                frame=i,
                timestamp=t,
                players=players,
                global_motion_energy=min(1.0, abs(players[0].velocity[0]) * 2.0),
                collective_centroid=(cx, cy),
                collective_direction=(players[0].velocity[0], 0.0),
                collective_density=0.4,
                event_refs=[0] if i == 20 else [],
            )
        )

    manifest = default_manifest(
        sourceId=source_id,
        packageId=package_id,
        fps=fps,
        frameCount=frame_count,
        durationSeconds=frame_count / fps,
        sourceWidth=width,
        sourceHeight=height,
    )
    return PdjvPackage(
        manifest=manifest,
        frames=frames,
        events=events,
        validation={
            "backend": "stdlib-synthetic",
            "notes": "Global depth crops share one clip-normalized range. Field coordinates use optional homography.",
            "stableIds": [7, 11],
        },
    )


def analyze_to_package(clip: Path, dest: Path, max_frames: int = 90) -> Path:
    info = inspect_clip(clip)
    frames = min(int(info["frameCount"]), max_frames)
    package = synthesize_package(
        source_id=source_id_for(clip, info["width"], info["height"], info["fps"], frames),
        width=info["width"],
        height=info["height"],
        fps=info["fps"],
        frame_count=max(8, frames if frames < 10000 else 90),
        package_id=source_id_for(clip, info["width"], info["height"], info["fps"], frames),
        occlude_from=30,
    )
    package.validation["clipInspection"] = info
    package.validation["models"] = {
        "detector": "adapter-synthetic",
        "tracker": "persistent-id-synthetic",
        "segmentation": "ellipse-confidence",
        "pose": "pdj-pose-17-kinematic",
        "depth": "full-frame-normalized-then-cropped",
        "field": "optional-identity-homography",
        "license": "generated-in-repo, no third-party weights",
    }
    export_package(package, dest)
    return dest
