# Adapter notes

The stdlib pipeline used for the first spike does not download neural weights.

| Stage | Current adapter | License |
|---|---|---|
| Clip inspect | ffprobe if present, else defaults | FFmpeg LGPL/GPL if used |
| Detect / track | deterministic synthetic players | generated in-repo |
| Mask | ellipse confidence crop | generated in-repo |
| Pose | `pdj-pose-17` kinematic layout | generated in-repo |
| Depth | full-frame normalized field, then crop | generated in-repo |
| Field | optional identity homography | generated in-repo |

Replace adapters with Apple-Silicon-compatible models before production analysis. Record version, license, and purpose in `validation.json` → `models`.
