# Legacy four-channel smoke test — 2026-09-01

Built in isolated git worktree `/tmp/pdj-legacy-four-channel-v1` at commit `5eef6e7` so the dirty eight-channel working tree was not modified.

## Compile

`make -j8` succeeded. Target: `bin/pdj-legacy-four-channel-v1.app` (Mach-O arm64).

## Launch

Launched from `bin/` so `ofLoadJson("settings.json")` resolved `bin/data/settings.json`.

Observed:

- ClipPool found 48 clips
- OSCSender: Sending to localhost:9001
- Channels 0-3 each loaded a video plan (Short/Full)
- AVFoundation loaded four 1080x1920 clips at ~23.976 fps
- Process remained running for the 8-second smoke window and was then terminated

A desktop screenshot was captured while the process was alive (`notes/control-ui.png`).

## Source inventory gate

`src/` at `5eef6e7` contains only the four-channel set (Channel, CVPipeline, GraphicScore, GlobalDirector, VideoDirector, ControlApp, ofApp, OSC, EventDetector, ClipPool). No PresentationApp, VisualComposer, VisualGenerator, PerformanceMonitor, SettingsStore, analyzer/, volumetric/, or PDJV files.

## Known limitations

- Architecture assumes one process per four channels / proposed two-computer layout
- Settings load from CWD-relative `settings.json`, not Application Support
- Video library is external and gitignored
- SuperCollider was not evaluated during this compile smoke test; OSC destination was configured
