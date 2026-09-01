# Eight-channel foundation smoke test — 2026-09-01

Validated the dirty working tree on `feature/eight-channel-foundation` after archiving `5eef6e7` separately.

## Compile

`make -j8` succeeded. Linked `PresentationApp`, `VisualComposer`, `VisualGenerator`, `PerformanceMonitor`, and eight `Channel` instances.

## Launch (`outputMode: dualWindow8`)

- ClipPool found 48 clips
- OSCSender: Sending to localhost:9001
- Channels 0-7 each received a VideoDirector plan
- Eight 1080x1920 AVFoundation loads completed
- Process remained running for a 10-second smoke window

`settings.json` records ICUIXIAN 0104-XZ 4x1 @ 1920x1080 / 60 Hz / 90° and `performanceTest` thresholds. The 10-minute stress test was not executed in this preservation pass; PerformanceMonitor compiled and settings are present.

## Audit

Included: eight-channel presentation, SettingsStore, VisualComposer/Generator, PerformanceMonitor, packaging scripts, SuperCollider OSC updates, planning specs already in the working tree.

Excluded from git: archive `.app` binaries, video clips, `obj/`, `dist/`.
