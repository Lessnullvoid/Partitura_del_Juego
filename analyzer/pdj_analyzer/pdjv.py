"""PDJV v0 export, load, and validation (stdlib only)."""

from __future__ import annotations

import json
import math
import os
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple

INDEX_MAGIC = b"PDJI"
INDEX_VERSION = 0
SUPPORTED_MAJORS = {0, 1}


def _u16(v: int) -> bytes:
    return struct.pack("<H", v)


def _u32(v: int) -> bytes:
    return struct.pack("<I", v)


def _i32(v: int) -> bytes:
    return struct.pack("<i", v)


def _u64(v: int) -> bytes:
    return struct.pack("<Q", v)


def _f32(v: float) -> bytes:
    return struct.pack("<f", float(v))


def _f64(v: float) -> bytes:
    return struct.pack("<d", float(v))


@dataclass
class SpatialBlock:
    width: int
    height: int
    data: bytes


@dataclass
class PlayerRecord:
    tracking_id: int
    team_id: int = -1
    observed: bool = True
    confidence: float = 1.0
    image_centroid: Tuple[float, float] = (0.5, 0.5)
    velocity: Tuple[float, float] = (0.0, 0.0)
    acceleration: Tuple[float, float] = (0.0, 0.0)
    bounding_box: Tuple[float, float, float, float] = (0.4, 0.3, 0.15, 0.4)
    field_position: Tuple[float, float] = (0.0, 0.0)
    ground_contact: Tuple[float, float] = (0.0, 0.0)
    estimated_scale: float = 1.0
    field_confidence: float = 0.0
    joints: List[Tuple[float, float, float, float]] = field(default_factory=list)
    mask: Optional[SpatialBlock] = None
    depth: Optional[SpatialBlock] = None
    motion: Optional[SpatialBlock] = None


@dataclass
class FrameRecord:
    frame: int
    timestamp: float
    players: List[PlayerRecord]
    global_motion_energy: float = 0.0
    collective_centroid: Tuple[float, float] = (0.5, 0.5)
    collective_direction: Tuple[float, float] = (0.0, 0.0)
    collective_density: float = 0.0
    event_refs: List[int] = field(default_factory=list)


@dataclass
class PdjvPackage:
    manifest: Dict[str, Any]
    frames: List[FrameRecord]
    events: List[Dict[str, Any]] = field(default_factory=list)
    validation: Dict[str, Any] = field(default_factory=dict)


def default_manifest(**overrides: Any) -> Dict[str, Any]:
    manifest = {
        "format": "PDJV",
        "version": 1,
        "sourceId": "synthetic",
        "analysisId": "stdlib-synthetic-v0",
        "packageId": "golden-v0",
        "fps": 30.0,
        "frameCount": 1,
        "durationSeconds": 1.0 / 30.0,
        "sourceWidth": 1920,
        "sourceHeight": 1080,
        "coordinateSystem": "normalized_image",
        "depthConvention": "larger_means_farther",
        "jointSchema": "pdj-pose-17",
        "endianness": "little",
        "features": {
            "masks": True,
            "depth": True,
            "pose": True,
            "motion": True,
            "events": True,
            "teamLabels": False,
            "fieldCalibration": True,
        },
        "depth": {
            "convention": "larger_means_farther",
            "globalMin": 0.0,
            "globalMax": 1.0,
            "invalidValue": 0,
            "hasPlayerRelativeDetail": False,
        },
        "field": {
            "available": True,
            "homography": [1, 0, 0, 0, 1, 0, 0, 0, 1],
            "units": "normalized_pitch",
        },
        "compression": {
            "observations": "pdjv-v0-le",
            "masks": "u8-raw",
            "depth": "u16-global-raw",
            "motion": "i16-grid-raw",
        },
    }
    manifest.update(overrides)
    return manifest


def _pack_player(player: PlayerRecord, mask_off: int, depth_off: int, motion_off: int) -> bytes:
    mask = player.mask or SpatialBlock(0, 0, b"")
    depth = player.depth or SpatialBlock(0, 0, b"")
    motion = player.motion or SpatialBlock(0, 0, b"")
    blob = b"".join(
        (
            _i32(player.tracking_id),
            _i32(player.team_id),
            struct.pack("<B", 1 if player.observed else 0),
            _f32(player.confidence),
            _f32(player.image_centroid[0]),
            _f32(player.image_centroid[1]),
            _f32(player.velocity[0]),
            _f32(player.velocity[1]),
            _f32(player.acceleration[0]),
            _f32(player.acceleration[1]),
            _f32(player.bounding_box[0]),
            _f32(player.bounding_box[1]),
            _f32(player.bounding_box[2]),
            _f32(player.bounding_box[3]),
            _f32(player.field_position[0]),
            _f32(player.field_position[1]),
            _f32(player.ground_contact[0]),
            _f32(player.ground_contact[1]),
            _f32(player.estimated_scale),
            _f32(player.field_confidence),
            _u32(mask_off),
            _u32(len(mask.data)),
            _u32(mask.width),
            _u32(mask.height),
            _u32(depth_off),
            _u32(len(depth.data)),
            _u32(depth.width),
            _u32(depth.height),
            _u32(motion_off),
            _u32(len(motion.data)),
            _u32(motion.width),
            _u32(motion.height),
            _u16(len(player.joints)),
        )
    )
    for joint in player.joints:
        blob += b"".join(_f32(v) for v in joint)
    return blob


def export_package(package: PdjvPackage, dest: Path) -> Path:
    dest = Path(dest)
    dest.mkdir(parents=True, exist_ok=True)
    masks = bytearray()
    depth = bytearray()
    motion = bytearray()
    observations = bytearray()
    index = bytearray(INDEX_MAGIC + _u16(INDEX_VERSION) + _u32(len(package.frames)))
    jsonl: List[str] = []

    package.manifest["frameCount"] = len(package.frames)
    fps = float(package.manifest["fps"])
    package.manifest["durationSeconds"] = (
        len(package.frames) / fps if fps > 0 else 0.0
    )

    for frame in package.frames:
        start = len(observations)
        observations += _u32(len(frame.players))
        observations += _f32(frame.global_motion_energy)
        observations += _f32(frame.collective_centroid[0])
        observations += _f32(frame.collective_centroid[1])
        observations += _f32(frame.collective_direction[0])
        observations += _f32(frame.collective_direction[1])
        observations += _f32(frame.collective_density)
        observations += _u32(len(frame.event_refs))
        for ref in frame.event_refs:
            observations += _i32(ref)

        player_json = []
        for player in frame.players:
            mask_off = len(masks)
            depth_off = len(depth)
            motion_off = len(motion)
            if player.mask:
                masks += player.mask.data
            if player.depth:
                depth += player.depth.data
            if player.motion:
                motion += player.motion.data
            observations += _pack_player(player, mask_off, depth_off, motion_off)
            player_json.append(
                {
                    "trackingId": player.tracking_id,
                    "observed": player.observed,
                    "confidence": player.confidence,
                    "imageCentroid": list(player.image_centroid),
                    "velocity": list(player.velocity),
                    "boundingBox": list(player.bounding_box),
                    "fieldPosition": list(player.field_position),
                    "groundContact": list(player.ground_contact),
                    "fieldConfidence": player.field_confidence,
                    "joints": [list(j) for j in player.joints],
                    "mask": None
                    if not player.mask
                    else {
                        "offset": mask_off,
                        "width": player.mask.width,
                        "height": player.mask.height,
                    },
                    "depth": None
                    if not player.depth
                    else {
                        "offset": depth_off,
                        "width": player.depth.width,
                        "height": player.depth.height,
                    },
                }
            )
        size = len(observations) - start
        index += _i32(frame.frame) + _f64(frame.timestamp) + _u64(start) + _u32(size)
        jsonl.append(
            json.dumps(
                {
                    "frame": frame.frame,
                    "timestamp": frame.timestamp,
                    "players": player_json,
                    "collective": {
                        "centroid": list(frame.collective_centroid),
                        "direction": list(frame.collective_direction),
                        "density": frame.collective_density,
                        "motionEnergy": frame.global_motion_energy,
                    },
                    "eventRefs": frame.event_refs,
                },
                separators=(",", ":"),
            )
        )

    (dest / "manifest.json").write_text(json.dumps(package.manifest, indent=2) + "\n")
    (dest / "frame_index.bin").write_bytes(index)
    (dest / "observations.bin").write_bytes(observations)
    (dest / "observations.jsonl").write_text("\n".join(jsonl) + "\n")
    (dest / "masks.bin").write_bytes(bytes(masks))
    (dest / "depth.bin").write_bytes(bytes(depth))
    (dest / "motion.bin").write_bytes(bytes(motion))
    (dest / "events.json").write_text(json.dumps({"events": package.events}, indent=2) + "\n")
    (dest / "validation.json").write_text(json.dumps(package.validation, indent=2) + "\n")
    return dest


def load_manifest(path: Path) -> Dict[str, Any]:
    return json.loads(Path(path).read_text())


class _Reader:
    def __init__(self, data: bytes):
        self.data = data
        self.o = 0

    def take(self, n: int) -> bytes:
        if self.o + n > len(self.data):
            raise ValueError("truncated PDJV record")
        chunk = self.data[self.o : self.o + n]
        self.o += n
        return chunk

    def u16(self) -> int:
        return struct.unpack("<H", self.take(2))[0]

    def u32(self) -> int:
        return struct.unpack("<I", self.take(4))[0]

    def i32(self) -> int:
        return struct.unpack("<i", self.take(4))[0]

    def u64(self) -> int:
        return struct.unpack("<Q", self.take(8))[0]

    def f32(self) -> float:
        return struct.unpack("<f", self.take(4))[0]

    def f64(self) -> float:
        return struct.unpack("<d", self.take(8))[0]


def read_index(path: Path) -> List[Tuple[int, float, int, int]]:
    raw = Path(path).read_bytes()
    r = _Reader(raw)
    if r.take(4) != INDEX_MAGIC:
        raise ValueError("invalid frame_index magic")
    version = r.u16()
    if version != INDEX_VERSION:
        raise ValueError(f"unsupported frame_index version {version}")
    count = r.u32()
    rows = []
    for _ in range(count):
        rows.append((r.i32(), r.f64(), r.u64(), r.u32()))
    return rows


def read_frame(obs: bytes, offset: int, size: int) -> FrameRecord:
    r = _Reader(obs[offset : offset + size])
    player_count = r.u32()
    energy = r.f32()
    cx, cy = r.f32(), r.f32()
    dx, dy = r.f32(), r.f32()
    density = r.f32()
    event_count = r.u32()
    refs = [r.i32() for _ in range(event_count)]
    players = []
    for _ in range(player_count):
        tracking = r.i32()
        team = r.i32()
        observed = r.take(1)[0] == 1
        confidence = r.f32()
        image = (r.f32(), r.f32())
        vel = (r.f32(), r.f32())
        acc = (r.f32(), r.f32())
        bbox = (r.f32(), r.f32(), r.f32(), r.f32())
        field_p = (r.f32(), r.f32())
        ground = (r.f32(), r.f32())
        scale = r.f32()
        field_c = r.f32()
        mask = (r.u32(), r.u32(), r.u32(), r.u32())
        depth = (r.u32(), r.u32(), r.u32(), r.u32())
        motion = (r.u32(), r.u32(), r.u32(), r.u32())
        joint_count = r.u16()
        joints = [(r.f32(), r.f32(), r.f32(), r.f32()) for _ in range(joint_count)]
        players.append(
            PlayerRecord(
                tracking_id=tracking,
                team_id=team,
                observed=observed,
                confidence=confidence,
                image_centroid=image,
                velocity=vel,
                acceleration=acc,
                bounding_box=bbox,
                field_position=field_p,
                ground_contact=ground,
                estimated_scale=scale,
                field_confidence=field_c,
                joints=joints,
                mask=SpatialBlock(mask[2], mask[3], b"") if mask[1] else None,
                depth=SpatialBlock(depth[2], depth[3], b"") if depth[1] else None,
                motion=SpatialBlock(motion[2], motion[3], b"") if motion[1] else None,
            )
        )
    return FrameRecord(
        frame=0,
        timestamp=0.0,
        players=players,
        global_motion_energy=energy,
        collective_centroid=(cx, cy),
        collective_direction=(dx, dy),
        collective_density=density,
        event_refs=refs,
    )


def validate_package(path: Path) -> List[str]:
    root = Path(path)
    errors: List[str] = []
    required = (
        "manifest.json",
        "frame_index.bin",
        "observations.bin",
        "masks.bin",
        "depth.bin",
        "motion.bin",
        "events.json",
        "validation.json",
    )
    for name in required:
        if not (root / name).is_file():
            errors.append(f"missing {name}")
    if errors:
        return errors

    if (root / "thumbnail.png").exists():
        errors.append("production package must not contain thumbnail.png")

    try:
        manifest = load_manifest(root / "manifest.json")
    except json.JSONDecodeError as exc:
        return [f"manifest.json: {exc}"]

    if manifest.get("format") != "PDJV":
        errors.append("format must be PDJV")
    version = manifest.get("version")
    if version not in SUPPORTED_MAJORS:
        errors.append(f"unsupported major version {version}")
    if manifest.get("endianness") != "little":
        errors.append("endianness must be little")
    if manifest.get("coordinateSystem") != "normalized_image":
        errors.append("coordinateSystem must be normalized_image")
    depth = manifest.get("depth") or {}
    if depth.get("convention") not in ("larger_means_farther", "larger_means_nearer"):
        errors.append("depth.convention is missing or invalid")
    if float(depth.get("globalMax", 0)) <= float(depth.get("globalMin", 0)):
        errors.append("depth global range is invalid")

    try:
        rows = read_index(root / "frame_index.bin")
    except ValueError as exc:
        errors.append(str(exc))
        return errors

    obs = (root / "observations.bin").read_bytes()
    masks = (root / "masks.bin").read_bytes()
    depths = (root / "depth.bin").read_bytes()
    motions = (root / "motion.bin").read_bytes()
    if len(rows) != int(manifest.get("frameCount", -1)):
        errors.append("frame_index count does not match manifest.frameCount")

    seen_ids: Dict[int, int] = {}
    for frame, timestamp, offset, size in rows:
        if offset + size > len(obs):
            errors.append(f"frame {frame}: observation range out of bounds")
            continue
        if timestamp < 0 or math.isnan(timestamp):
            errors.append(f"frame {frame}: invalid timestamp")
        parsed = read_frame(obs, offset, size)
        for player in parsed.players:
            if player.tracking_id < 0:
                errors.append(f"frame {frame}: negative tracking id")
            first = seen_ids.setdefault(player.tracking_id, frame)
            if first > frame:
                errors.append(f"tracking id {player.tracking_id} reused after retirement")
            if player.mask and player.mask.width * player.mask.height:
                # los offsets no se guardan en el SpatialBlock reconstruido; releer el binario en crudo
                pass
        # límites de los archivos espaciales usando de nuevo el registro binario
        r = _Reader(obs[offset : offset + size])
        count = r.u32()
        r.f32(); r.f32(); r.f32(); r.f32(); r.f32(); r.f32()
        ev = r.u32()
        for _ in range(ev):
            r.i32()
        for _ in range(count):
            r.i32(); r.i32(); r.take(1)
            for _n in range(17):
                r.f32()
            mask_off, mask_size, mw, mh = r.u32(), r.u32(), r.u32(), r.u32()
            depth_off, depth_size, dw, dh = r.u32(), r.u32(), r.u32(), r.u32()
            motion_off, motion_size, ow, oh = r.u32(), r.u32(), r.u32(), r.u32()
            joints = r.u16()
            for _j in range(joints):
                r.f32(); r.f32(); r.f32(); r.f32()
            if mask_off + mask_size > len(masks) or (mask_size and mask_size != mw * mh):
                errors.append(f"frame {frame}: invalid mask block")
            if depth_off + depth_size > len(depths) or (depth_size and depth_size != dw * dh * 2):
                errors.append(f"frame {frame}: invalid depth block")
            if motion_off + motion_size > len(motions) or (
                motion_size and motion_size != ow * oh * 4
            ):
                errors.append(f"frame {frame}: invalid motion block")
    return errors
