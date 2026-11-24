"""net_shape.py：ネットの上辺の高さ、分割、揺れの重み、本体と白帯の面。"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tennis_blender import dims as d  # noqa: E402
from tennis_blender import net_shape as ns  # noqa: E402

TOL = 1e-9


def _normal(verts, face):
    """四角形の面の法線（最初の 3 頂点から）。"""
    (ax, ay, az), (bx, by, bz), (cx, cy, cz) = (verts[i] for i in face[:3])
    u = (bx - ax, by - ay, bz - az)
    v = (cx - bx, cy - by, cz - bz)
    return (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])


class TopHeightTest(unittest.TestCase):
    def test_key_points(self):
        self.assertAlmostEqual(ns.top_height(0.0), 0.914, delta=TOL)
        self.assertAlmostEqual(ns.top_height(5.029), 1.07, delta=TOL)
        self.assertAlmostEqual(ns.top_height(-5.029), 1.07, delta=TOL)
        self.assertAlmostEqual(ns.top_height(6.399), 1.07, delta=TOL)   # スティック → ポストは一定
        self.assertAlmostEqual(ns.top_height(5.029 / 2), (0.914 + 1.07) / 2, delta=TOL)


class ColumnYsTest(unittest.TestCase):
    def test_64_columns(self):
        ys = ns.column_ys(64)
        self.assertEqual(len(ys), 65)
        self.assertAlmostEqual(ys[0], -6.399, delta=TOL)
        self.assertAlmostEqual(ys[-1], 6.399, delta=TOL)
        for key in (0.0, 5.029, -5.029):   # 上辺が折れる所と中央に頂点がある
            self.assertTrue(any(abs(y - key) < TOL for y in ys), key)
        steps = [b - a for a, b in zip(ys, ys[1:])]
        self.assertTrue(all(0.19 < s < 0.21 for s in steps), (min(steps), max(steps)))
        for a, b in zip(ys, reversed(ys)):
            self.assertAlmostEqual(a, -b, delta=TOL)

    def test_rejects_bad_segments(self):
        for bad in (0, 2, 3, 63):
            with self.assertRaises(ValueError):
                ns.column_ys(bad)


class SwayWeightTest(unittest.TestCase):
    def test_boundaries(self):
        self.assertAlmostEqual(ns.sway_weight(0.0, 0.914), 1.0, delta=TOL)   # 上辺の中央
        self.assertAlmostEqual(ns.sway_weight(3.0, 0.0), 0.0, delta=TOL)     # 下辺
        self.assertAlmostEqual(ns.sway_weight(6.399, 1.07), 0.0, delta=TOL)  # ポスト際
        self.assertAlmostEqual(ns.sway_weight(-6.399, 0.5), 0.0, delta=TOL)

    def test_range_over_grid(self):
        verts, _ = ns.net_grid(d.NET_SEGMENTS_Y, d.NET_SEGMENTS_Z)
        weights = [ns.sway_weight(y, z) for _, y, z in verts]
        self.assertGreaterEqual(min(weights), 0.0)
        self.assertLessEqual(max(weights), 1.0)
        self.assertAlmostEqual(max(weights), 1.0, delta=TOL)


class NetGridTest(unittest.TestCase):
    def test_counts(self):
        verts, faces = ns.net_grid(64, 12)
        self.assertEqual((len(verts), len(faces)), (65 * 13, 64 * 12))

    def test_top_edge_follows_profile_and_bottom_on_ground(self):
        verts, _ = ns.net_grid(64, 12)
        for k in range(65):
            column = verts[k * 13:(k + 1) * 13]
            y = column[0][1]
            self.assertAlmostEqual(column[0][2], 0.0, delta=TOL)
            self.assertAlmostEqual(column[-1][2], ns.top_height(y), delta=TOL)
        self.assertTrue(all(x == 0.0 for x, _, _ in verts))

    def test_normals_face_plus_x(self):
        verts, faces = ns.net_grid(64, 12)
        self.assertTrue(all(_normal(verts, f)[0] > 0 for f in faces))

    def test_rejects_bad_segments_z(self):
        with self.assertRaises(ValueError):
            ns.net_grid(64, 0)


class BandTest(unittest.TestCase):
    def test_counts_and_extent(self):
        verts, faces = ns.band(64)
        self.assertEqual((len(verts), len(faces)), (65 * 4, 64 * 3))
        self.assertAlmostEqual(max(x for x, _, _ in verts), 0.004, delta=TOL)
        self.assertAlmostEqual(min(x for x, _, _ in verts), -0.004, delta=TOL)
        self.assertAlmostEqual(min(z for _, _, z in verts), 0.914 - 0.05, delta=TOL)
        self.assertAlmostEqual(max(z for _, _, z in verts), 1.07, delta=TOL)

    def test_normals_face_outward(self):
        verts, faces = ns.band(64)
        for k, f in enumerate(faces):
            n = _normal(verts, f)
            side = k % 3   # +X 側・上面・-X 側
            if side == 0:
                self.assertGreater(n[0], 0)
            elif side == 1:
                self.assertGreater(n[2], 0)
            else:
                self.assertLess(n[0], 0)


if __name__ == "__main__":
    unittest.main()
