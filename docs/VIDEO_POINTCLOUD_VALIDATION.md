# Eight-channel video point-cloud validation

Date: 2026-09-01

## Foundation

- Branch ancestry resolves to tag `pdj-eight-channel-foundation-v1`, commit
  `2a9d3da`.
- The preserved baseline record is
  `archives/eight-channel-foundation-v1/notes/TEST.md`.
- Baseline evidence records 48 clips, OSC output on port 9001, channels 0–7,
  and `dualWindow8`.

## Current runtime

- Release build: passed.
- Two presentation groups launched with channels 0–3 and 4–7.
- All eight 1080×1920 videos loaded.
- OSC sender initialized on port 9001 and receiver on port 9002.
- A per-channel `/pdjv/channel/0/vpc/depthScale` update was accepted while the
  runtime remained active.
- Point-cloud shaders compiled without errors.
- Packaged application remained active during a post-package launch smoke test.

The 60-second LaunchServices performance run used eight 256×256 point clouds:

- Window A: 30.25 fps average, 1.66 ms latest GPU sample.
- Window B: 30.31 fps average, 1.25 ms latest GPU sample.
- Resident memory growth: -199.36 MB after warmup.
- Slowest measured subsystem: channel draw, 1.15 ms average.

The report is stored at
`~/Documents/PartituraDelJuego/performance_reports/performance_20260901_171708.json`.
Its strict result remains `FAIL` because clip transitions produced a small
number of late/stall frames and Window A's p95 exceeded 38 ms. Sustained
average frame rate met the 30 fps installation target.

Short debugger-instrumented runs covered 128², 160², 256², and 384² grids.
Grid size was not the measured bottleneck; 256² remains the installation
default.

## Packaging

`scripts/package_macos_arm64.sh` built, signed, extracted, and revalidated:

- `dist/Partitura_del_Juego-macOS-arm64.zip`
- `dist/Partitura_del_Juego-macOS-arm64.zip.sha256`

The package contains the 48-clip library, point-cloud shaders, `dualWindow8`
settings, SuperCollider compatibility files, and optional PDJV data when
available. The SHA-256 verification passed.

The software mapping preserves the validated ICUIXIAN order. Final electrical
confirmation still requires the two physical controllers and eight displays.

## Generative intercalation sequence

The live OSC validator
`scripts/validate_intercalation_runtime.py` launched the release application
twice with the same pinned seed (`PDJ_COMPOSER_SEED`). Both runs observed:

- a seeded opening moment rather than a fixed one, reaching `Intercalation`
  without leaving it again; the recorded run was
  `BarScan -> Pulse -> Intercalation`;
- protected dwell for every unified system moment that ran;
- no more than two point-cloud video channels in either four-channel group;
- the same intercalation video assignment across both runs.

The shipped `visualComposer.seed` is `0`, which draws a fresh seed per launch so
the installation does not replay the same evening; any non-zero seed restores
exact repeatability.

Slice adjacency was measured separately from the same OSC stream over 1560
complete intercalation frames: no two neighbouring slices ran the same
generator, apart from a single frame at a reassignment boundary. Unified system
moments and global takeovers put the whole wall on one generator by design and
are exempt from the rule.

Visual captures from `dualWindow8` confirmed asynchronous monochrome square-cell
pulses, independently moving white/grey luminous bar scans, and an intercalation frame
with three generator segments plus one recognizable point-cloud video segment
in presentation group A. Empty areas remained black and the glow pass did not
accumulate between frames. Granular Raster and Orbital Rings are also available
as deterministic modes 10 and 11, but Orbital Rings is listed in
`visualComposer.disabledGenerators`, so the program never schedules it; a
60-second OSC capture reported modes 8, 9, and 10 only. Clearing that list
returns it to the rotation.

A 25-second 256x256 accelerated-sequence profile exercised Pulse, BarScan,
Intercalation, video loading, and a global takeover:

- Window A: 29.84 fps average.
- Window B: 29.85 fps average.
- Memory growth after warmup: -334.13 MB.
- Slowest measured channel before the final profiling-label fix: 1.02 ms
  average.

The report is
`~/Documents/PartituraDelJuego/performance_reports/performance_20260901_175627.json`.
The strict result remains `FAIL`: clip loads produced two roughly 200 ms stalls
per window, and Window A's p95 was 43.54 ms. This matches the pre-intercalation
profile's existing clip-transition limitation rather than sustained GPU load.

The run emitted one macOS OpenGL driver compatibility warning for a rectangular
texture sampler, but shader compilation succeeded and Pulse, BarScan, and
point-cloud output were all visibly present.
