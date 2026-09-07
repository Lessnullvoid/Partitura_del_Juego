# pdj-volumetric

Installation runtime for PDJV packages. It does not decode source video and does not run OpenCV.

## Build

```bash
cd volumetric
make -j8
cd bin && ./volumetric.app/Contents/MacOS/volumetric
```

Requires openFrameworks 0.12.0 at the `OF_ROOT` in `config.make`. Addons: ofxOsc, ofxImGui. OpenGL 4.1, no compute shaders.

## Data

`bin/data/golden_v0.pdjv` is the Temporal Echo slice fixture. `golden_v1.pdjv` is the frozen production layout (same binary records, manifest `version: 1`, field homography). Neither package contains source video.

Point at another package with `settings.json` → `pdjv.primary`. OSC listen port 9002 accepts `/pdjv/generator`, `/pdjv/preset`, `/pdjv/play`.

## Tests

```bash
make -C volumetric/tests
```

## Output

`outputMode: dualWindow8` creates two 1920×1080 presentation windows (groups A/B, separate HDR/feedback FBOs) plus a control window. OSC: `/pdjv/channel/N/...` with compatibility `/pdj/channel/N/...`.
