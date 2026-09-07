# Volumetric Runtime V1 — slice notes

Tagged: `pdj-volumetric-v1` on `feature/volumetric-runtime` (2026-09-01).

## V1 acceptance checklist

- [x] Legacy recoverable via tags (`pdj-legacy-four-channel-v1`, `pdj-eight-channel-foundation-v1`)
- [x] Analyzer produces validated PDJV (synthetic 96×192 human silhouette + neural `real_pipeline.py`)
- [x] No runtime video/CV
- [x] Players always volumetric (wireframe / splats / points — never 2D video)
- [x] Stable IDs across occlusion gaps (synthetic gap frame 30–35 on tracking id 11)
- [x] Depth + pose drive surface (`PlayerSurfaceMesh` mask walk + anatomical tubes)
- [x] GPU fragment sim ping-pong (`simulate.frag`, no readback; CPU spring fallback)
- [x] HDR emissive extract, bloom, tonemap composite, bounded feedback (emissive-only history)
- [x] Six generators: WireframeShell + five luminous modes (three presets each)
- [x] Eight-screen content modes via `VolumetricContentDirector` (Independent, FourPlusFour, Mixed, Synchronized, SharedView)
- [x] OSC `/pdjv/channel/N/...` + legacy `/pdj/...`; inbound `/pdjv/generator|preset|play|camera/mode`
- [x] Installation tier: 6000 pts/player, 48×96 mask grid, multiscale bloom
- [x] Graceful corrupt-data handling (reader errors surfaced in UI; truncated frames rejected)

## Architecture

- **Mesh core:** `volumetric/src/volume/PlayerSurfaceMesh.{h,cpp}` — pose-shell rings + mask-grid UV topology, stable point IDs, wire index buffers
- **Engine:** `VolumetricEngine` delegates target/update/draw to mesh; portrait bust + full-body camera presets
- **Render:** geometry FBO → emissive bloom → tonemap composite → feedback; wire + splat + point paths
- **Director:** `VolumetricComposer` timed stage rotation (optional via `composerEnabled`)
- **Analyzer:** `analyzer/pdj_analyzer/pipeline.py` (synthetic), `real_pipeline.py` (YOLO11 seg/pose + Depth Anything V2)
- **Fixtures:** `docs/pdjv/fixtures/golden_v0.pdjv`, `golden_v1.pdjv` (96×192 masks, no thumbnail)

## Run

```bash
cd volumetric && make -j8
cd bin && ./volumetric.app/Contents/MacOS/volumetric
```

Default package: `golden_v0.pdjv`, default generator: `WireframeShell` preset 1 (mask-grid wireframe).

## Package

```bash
scripts/package_volumetric_macos_arm64.sh
```

Bundles app, shaders, golden fixture, settings — no source video.

## Neural export

```bash
cd analyzer
python3 scripts/analyze_clip.py --clip /path/to/clip.mp4 --out /path/to/out.pdjv --backend real
```
