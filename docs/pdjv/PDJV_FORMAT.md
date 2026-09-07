# PDJV format

PDJV is the versioned, source-video-independent package between `pdj-analyzer` and `pdj-volumetric`.

This document freezes package architecture, endianness, timestamps, coordinates, versioning, and bounds rules. Mask resolution, exact depth encoding, motion layout, anatomical packing, confidence packing, and interpolation fields are **provisional in v0** and are frozen only in PDJV v1 after the vertical-slice review.

## Compatibility

- `format` must be `PDJV`.
- `version` is an integer major version. The runtime rejects unsupported majors with a clear error.
- v0 is provisional (Temporal Echo slice). v1 is frozen: same little-endian records, optional field homography, globally normalized depth crops, no thumbnails.
- Optional feature flags and minor additive JSON fields must be skipped safely.
- All multi-byte integers and IEEE floats in `.bin` files are **little-endian**.

## Package directory

```text
clip_identifier.pdjv/
├── manifest.json
├── frame_index.bin
├── observations.bin
├── observations.jsonl      # v0 inspectable companion; optional in production
├── masks.bin
├── depth.bin
├── motion.bin
├── events.json
└── validation.json
```

Production packages contain **no source-image thumbnail**. Analyzer validation previews stay under `analyzer/output/` and are never shipped with the runtime.

## Manifest (stable)

Required keys:

| Key | Meaning |
|---|---|
| `format` | `"PDJV"` |
| `version` | `0` or `1` |
| `sourceId` | Deterministic source identifier |
| `analysisId` | Pipeline / model-set identifier |
| `packageId` | Stable package identity used in point hashes |
| `fps` | Nominal frames per second |
| `frameCount` | Integer frame count |
| `durationSeconds` | `frameCount / fps` |
| `sourceWidth`, `sourceHeight` | Source pixels; runtime never loads the pixels |
| `coordinateSystem` | `"normalized_image"` |
| `depthConvention` | `"larger_means_farther"` or `"larger_means_nearer"` |
| `jointSchema` | `"pdj-pose-17"` |
| `endianness` | `"little"` |
| `features` | Boolean map: `masks`, `depth`, `pose`, `motion`, `events`, `teamLabels`, `fieldCalibration` |
| `depth` | Global normalization block |
| `compression` | Codec identifiers |

### Coordinates

- Image X: 0 left, 1 right
- Image Y: 0 top, 1 bottom
- Time: seconds from clip start
- Velocity: normalized image units per second
- Pose joints: normalized in the full source frame, not the crop
- Field X/Y when present: 0–1 along the calibrated pitch, origin at the analyzed left/near corner

### Global depth

Depth is estimated on the **full frame**, temporally normalized at clip and frame level, then cropped through each player mask.

```json
"depth": {
  "convention": "larger_means_farther",
  "globalMin": 0.0,
  "globalMax": 1.0,
  "invalidValue": 0,
  "hasPlayerRelativeDetail": false
}
```

Cropped `uint16` blocks store normalized **global** depth. Optional player-relative detail, if present, is a second channel or sidecar and must not replace the global relationship.

### Optional field calibration

```json
"field": {
  "available": false,
  "homography": [1, 0, 0, 0, 1, 0, 0, 0, 1],
  "units": "normalized_pitch"
}
```

Close shots may omit field-space values. Missing field data does not invalidate a package.

## Frame index (`frame_index.bin`)

```text
magic[4] = "PDJI"
u16 version
u32 frameCount
repeat frameCount:
    i32 frame
    f64 timestampSeconds
    u64 observationOffset
    u32 observationSize
```

Offsets refer to `observations.bin`. The reader must reject out-of-range offsets.

## Observations (`observations.bin`, v0 provisional)

Each frame record:

```text
u32 playerCount
f32 globalMotionEnergy
f32 collectiveCentroidX, collectiveCentroidY
f32 collectiveDirectionX, collectiveDirectionY
f32 collectiveDensity
u32 eventRefCount
repeat eventRefCount:
    i32 eventIndex
repeat playerCount:
    i32 trackingId
    i32 teamId
    u8  observed            # 1 observed, 0 gap
    f32 confidence
    f32 imageCentroidX, imageCentroidY
    f32 velocityX, velocityY
    f32 accelerationX, accelerationY
    f32 boundingBoxX, Y, W, H   # normalized full-frame
    f32 fieldPositionX, fieldPositionY
    f32 groundContactX, groundContactY
    f32 estimatedScale
    f32 fieldConfidence
    u32 maskOffset, maskSize, maskWidth, maskHeight
    u32 depthOffset, depthSize, depthWidth, depthHeight
    u32 motionOffset, motionSize, motionWidth, motionHeight
    u16 jointCount
    repeat jointCount:
        f32 x, y, z, confidence
```

`observations.jsonl` is the inspectable v0 twin: one JSON object per frame, same semantics.

Spatial offsets refer to `masks.bin` (u8 confidence), `depth.bin` (u16 global normalized), and `motion.bin` (interleaved i16 dx, dy on a reduced grid). Crops must match the player bounding box aspect. Full-frame blocks per player are forbidden.

## Events (`events.json`)

```json
{
  "events": [
    {
      "id": 0,
      "timestamp": 1.2,
      "type": "acceleration_peak",
      "x": 0.4,
      "y": 0.6,
      "confidence": 0.8,
      "trackingIds": [3]
    }
  ]
}
```

## Lifecycle

Tracking IDs stay stable across short gaps. Retired IDs are not reused inside a package. The runtime distinguishes missing observation from player death.

## Anatomical stable points

```text
stablePointId = fnv1a64(packageId, trackingId, pointSeed)
```

Each persistent point also stores `bodyRegionId`, `poseSegment` (joint pair), `longitudinalCoordinate`, `radialCoordinate`, and `fallbackUV`. Reconstruction is pose-relative when segment confidence is sufficient; otherwise it uses mask, global depth, and fallback UV. Hash identity alone is not enough.

Body regions: `0 unknown`, `1 head`, `2 torso`, `3 left_arm`, `4 right_arm`, `5 left_leg`, `6 right_leg`.

Joint schema `pdj-pose-17`: nose, eyes, ears, shoulders, elbows, wrists, hips, knees, ankles.

## Sharing

Multiple channels that play the same package must share one `PdjvReader`, decoded spatial cache, and base timeline through `PdjvAssetPool`. Channels keep only camera, generator, render, viewport, and quality state.
