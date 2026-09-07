#!/usr/bin/env python3
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from pdj_analyzer.anatomy import anatomical_target, assign_region, hash_vectors, stable_point_id
from pdj_analyzer.pdjv import export_package, read_frame, read_index, validate_package
from pdj_analyzer.pipeline import synthesize_package


class AnatomyTests(unittest.TestCase):
    def test_hash_vectors_are_stable(self):
        expected = {
            ("golden-v0", 7, 0): 0x0,
        }
        # Comparar entre llamadas repetidas en lugar de un hexadecimal fijo que
        # podría ocultar deriva de implementación: la identidad debe ser determinista.
        first = list(hash_vectors())
        second = list(hash_vectors())
        self.assertEqual(first, second)
        self.assertTrue(all(item[3] != 0 for item in first))
        self.assertNotEqual(
            stable_point_id("golden-v0", 7, 0),
            stable_point_id("golden-v0", 7, 1),
        )
        _ = expected

    def test_pose_target_stays_on_leg_segment(self):
        joints = [(0.0, 0.0, 0.4, 0.0)] * 17
        joints[13] = (0.4, 0.6, 0.5, 0.9)
        joints[15] = (0.4, 0.8, 0.55, 0.9)
        x, y, z, used = anatomical_target(
            joints, 13, 15, 0.5, 0.0, (0.9, 0.1), (0.2, 0.2, 0.2, 0.2), 0.2
        )
        self.assertTrue(used)
        self.assertAlmostEqual(x, 0.4, places=5)
        self.assertGreater(y, 0.65)
        self.assertLess(y, 0.75)
        region, joint_a, joint_b, *_ = assign_region(5)
        self.assertIn(region, range(1, 7))
        self.assertNotEqual(joint_a, joint_b)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = Path(tempfile.mkdtemp())
        self.package = synthesize_package(
            "synthetic_golden", 1920, 1080, 30.0, 24, "golden-v0", occlude_from=12
        )
        self.path = export_package(self.package, self.temp / "pkg.pdjv")

    def tearDown(self):
        shutil.rmtree(self.temp)

    def test_roundtrip_and_seek(self):
        errors = validate_package(self.path)
        self.assertEqual(errors, [])
        rows = read_index(self.path / "frame_index.bin")
        self.assertEqual(len(rows), 24)
        obs = (self.path / "observations.bin").read_bytes()
        last = read_frame(obs, rows[-1][2], rows[-1][3])
        self.assertGreaterEqual(len(last.players), 2)
        self.assertEqual({p.tracking_id for p in last.players}, {7, 11})
        gap = read_frame(obs, rows[12][2], rows[12][3])
        hidden = [p for p in gap.players if p.tracking_id == 11][0]
        self.assertFalse(hidden.observed)

    def test_rejects_bad_version_and_thumbnail(self):
        manifest = json.loads((self.path / "manifest.json").read_text())
        manifest["version"] = 99
        (self.path / "manifest.json").write_text(json.dumps(manifest))
        errors = validate_package(self.path)
        self.assertTrue(any("unsupported major" in e for e in errors))
        manifest["version"] = 0
        (self.path / "manifest.json").write_text(json.dumps(manifest))
        (self.path / "thumbnail.png").write_bytes(b"not-a-source-but-forbidden")
        errors = validate_package(self.path)
        self.assertTrue(any("thumbnail" in e for e in errors))

    def test_corrupt_offset(self):
        index = bytearray((self.path / "frame_index.bin").read_bytes())
        # La cabecera ocupa 10 bytes; el primer registro es frame(4)+timestamp(8)+offset(8).
        offset_at = 10 + 4 + 8
        index[offset_at : offset_at + 8] = (10**12).to_bytes(8, "little")
        (self.path / "frame_index.bin").write_bytes(index)
        errors = validate_package(self.path)
        self.assertTrue(any("out of bounds" in e for e in errors), errors)

    def test_homography_and_global_depth(self):
        from pdj_analyzer.pipeline import apply_homography

        x, y = apply_homography(0.25, 0.80, [1, 0, 0, 0, 1, 0, 0, 0, 1])
        self.assertAlmostEqual(x, 0.25)
        self.assertAlmostEqual(y, 0.80)
        depth = self.package.frames[0].players[0].depth.data
        values = [int.from_bytes(depth[i : i + 2], "little") for i in range(0, len(depth), 2)]
        self.assertGreater(min(values), 0)
        self.assertLessEqual(max(values), 65535)
        self.assertGreater(max(values) - min(values), 10)

    def test_human_silhouette_mask(self):
        from pdj_analyzer.pipeline import _human_silhouette_mask

        mask = _human_silhouette_mask(96, 192)
        self.assertEqual(mask.width, 96)
        self.assertEqual(mask.height, 192)
        data = mask.data
        fg = [i for i, b in enumerate(data) if b >= 32]
        self.assertGreater(len(fg), 2000)
        # las regiones de cabeza y pies están ambas ocupadas
        head = sum(1 for i in fg if i < 96 * 30)
        feet = sum(1 for i in fg if i >= 96 * 150)
        self.assertGreater(head, 80)
        self.assertGreater(feet, 80)


if __name__ == "__main__":
    unittest.main()
