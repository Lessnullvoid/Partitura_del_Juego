"""Deterministic anatomical point identity shared with the C++ runtime."""

from __future__ import annotations

import math
import struct
from typing import Iterable, Sequence, Tuple

FNV64_OFFSET = 0xCBF29CE484222325
FNV64_PRIME = 0x100000001B3

JOINT_NAMES = (
    "nose",
    "left_eye",
    "right_eye",
    "left_ear",
    "right_ear",
    "left_shoulder",
    "right_shoulder",
    "left_elbow",
    "right_elbow",
    "left_wrist",
    "right_wrist",
    "left_hip",
    "right_hip",
    "left_knee",
    "right_knee",
    "left_ankle",
    "right_ankle",
)

BODY_REGIONS = {
    0: "unknown",
    1: "head",
    2: "torso",
    3: "left_arm",
    4: "right_arm",
    5: "left_leg",
    6: "right_leg",
}

POSE_SEGMENTS = (
    (0, 5, 1),   # nose-left_shoulder -> cabeza
    (0, 6, 1),
    (5, 6, 2),   # hombros -> torso
    (5, 11, 2),
    (6, 12, 2),
    (11, 12, 2),
    (5, 7, 3),
    (7, 9, 3),
    (6, 8, 4),
    (8, 10, 4),
    (11, 13, 5),
    (13, 15, 5),
    (12, 14, 6),
    (14, 16, 6),
)


def fnv1a64(data: bytes) -> int:
    h = FNV64_OFFSET
    for b in data:
        h ^= b
        h = (h * FNV64_PRIME) & 0xFFFFFFFFFFFFFFFF
    return h


def stable_point_id(package_id: str, tracking_id: int, point_seed: int) -> int:
    payload = package_id.encode("utf-8") + b"\0"
    payload += struct.pack("<ii", int(tracking_id), int(point_seed))
    return fnv1a64(payload)


def assign_region(seed: int) -> Tuple[int, int, int, float, float]:
    """Deterministic region + segment + body coordinates from a point seed."""
    region = 1 + (abs(seed) % 6)
    candidates = [s for s in POSE_SEGMENTS if s[2] == region]
    segment = candidates[abs(seed) % len(candidates)]
    longitudinal = ((abs(seed) * 17) % 1000) / 999.0
    radial = (((abs(seed) * 31) % 1000) / 999.0) * 2.0 - 1.0
    return region, segment[0], segment[1], longitudinal, radial


def anatomical_target(
    joints: Sequence[Sequence[float]],
    joint_a: int,
    joint_b: int,
    longitudinal: float,
    radial: float,
    fallback_uv: Sequence[float],
    bbox: Sequence[float],
    depth: float,
    min_confidence: float = 0.35,
) -> Tuple[float, float, float, bool]:
    """Return (x, y, z, used_pose). Joints are [x, y, z, c] in full-frame space."""
    if (
        0 <= joint_a < len(joints)
        and 0 <= joint_b < len(joints)
        and joints[joint_a][3] >= min_confidence
        and joints[joint_b][3] >= min_confidence
    ):
        ax, ay, az = joints[joint_a][0], joints[joint_a][1], joints[joint_a][2]
        bx, by, bz = joints[joint_b][0], joints[joint_b][1], joints[joint_b][2]
        t = max(0.0, min(1.0, longitudinal))
        px = ax + (bx - ax) * t
        py = ay + (by - ay) * t
        pz = az + (bz - az) * t
        dx, dy = bx - ax, by - ay
        nx, ny = -dy, dx
        length = (nx * nx + ny * ny) ** 0.5
        if length > 1e-6:
            nx, ny = nx / length, ny / length
            # La validación en Python usa un ángulo determinista derivado de la
            # coordenada radial suministrada. Los puntos del runtime almacenan una
            # segunda coordenada angular explícita para cubrir la cápsula por completo.
            angle = (abs(radial) * 13.0 % 1.0) * math.tau
            radius = math.sqrt(abs(radial)) * 0.04
            px += nx * math.cos(angle) * radius
            py += ny * math.cos(angle) * radius
            pz += math.sin(angle) * radius * 1.35
        return px, py, pz, True

    x = bbox[0] + fallback_uv[0] * bbox[2]
    y = bbox[1] + fallback_uv[1] * bbox[3]
    return x, y, depth, False


def hash_vectors() -> Iterable[Tuple[str, int, int, int]]:
    """Documented test vectors consumed by Python and C++."""
    cases = (
        ("golden-v0", 7, 0),
        ("golden-v0", 7, 1),
        ("match_001", 3, 42),
    )
    for package_id, tracking_id, seed in cases:
        yield package_id, tracking_id, seed, stable_point_id(package_id, tracking_id, seed)
