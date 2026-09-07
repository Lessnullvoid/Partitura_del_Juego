# PDJV v0 to v1

v1 freezes the v0 binary layout after the Temporal Echo prototype:

- little-endian `frame_index.bin` / `observations.bin`
- cropped u8 masks, u16 global depth, i16 motion grids
- optional field homography fields on each player
- anatomical reconstruction is a runtime concern; packages still store pose + crops

## Migration

- v0 packages remain readable.
- Exporters should write `"version": 1`.
- No field reordering. New optional JSON keys may appear in the manifest.
- Production packages still must not contain `thumbnail.png`.
