# Legacy four-channel build

Commit: `5eef6e7`
Branch: `archive/legacy-four-channel-v1`
Tag: `pdj-legacy-four-channel-v1`

## Environment recorded 2026-09-01

- macOS 26.5.2 (25F84), Darwin 25.5.0
- Apple Silicon arm64
- Xcode 26.6 (17F113)
- Apple clang 21.0.0 (clang-2100.1.1.101)
- openFrameworks 0.12.0 osx release at `/Users/microhm/Documents/of_v0.12.0_osx_release`
- Addons from that OF tree: ofxOpenCv, ofxCv (MIT), ofxOsc, ofxImGui

## Restore and compile

```bash
git fetch --tags
git checkout pdj-legacy-four-channel-v1
make
make RunRelease
```

`config.make` sets `OF_ROOT` and C++17. Video clips are not in git; place them at the `clips.folder` path from `bin/data/settings.json` (historically `../../cortos`).

This tag contains the four-channel / two-computer-era application only. It does not contain eight-channel presentation windows, VisualComposer, PerformanceMonitor, or any volumetric runtime files.
