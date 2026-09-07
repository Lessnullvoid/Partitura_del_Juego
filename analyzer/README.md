# pdj-analyzer

Offline analysis for *Partitura del Juego*. Produces PDJV packages. Model backends are replaceable adapters; they never become part of the installation runtime.

## Layout

- `pdj_analyzer/` — pipeline, PDJV export/validate, anatomical IDs, global depth, optional field homography
- `scripts/` — clip inspection, golden fixture export, package validation
- `tests/` — format, anatomy, and corruption tests (stdlib only)

## Environment

Pinned optional extras live in `requirements.txt`. The stdlib path can export and validate PDJV without NumPy or OpenCV. Heavy detectors, pose, and monocular depth remain adapter implementations.

Source media and source-derived validation previews stay in `output/` (gitignored). Production packages contain no source thumbnails.

```bash
python3 -m analyzer.tests.test_pdjv
python3 analyzer/scripts/export_golden.py
python3 analyzer/scripts/validate_package.py docs/pdjv/fixtures/golden_v0.pdjv
python3 analyzer/scripts/analyze_clip.py --clip ../../cortos/001.mp4 --out analyzer/output/spike.pdjv
```
