"""primitives.py：断面の多角形、筒・柱・箱の面の向きと体積、結合とスロットの番号。"""
import math
import sys
import unittest
from pathlib import Path

sys.path[:0] = [str(Path(__file__).resolve().parents[1]), str(Path(__file__).resolve().parent)]

from geometry_helpers import bounds, signed_volume  # noqa: E402
from tennis_blender import primitives as pr  # noqa: E402

TOL = 1e-9


class SectionTest(unittest.TestCase):
    def test_octagon_is_counterclockwise(self):
        pts = pr.octagon(0.005, 0.0115, 0.003)
        area = sum(u0 * v1 - u1 * v0 for (u0, v0), (u1, v1) in zip(pts, pts[1:] + pts[:1])) / 2
        self.assertGreater(area, 0.0)
        self.assertAlmostEqual(max(u for u, _ in pts), 0.005, delta=TOL)
        self.assertAlmostEqual(max(v for _, v in pts), 0.0115, delta=TOL)

    def test_regular_polygon_has_its_diameter_on_the_axes(self):
        pts = pr.regular_polygon(0.076, 16)
        self.assertEqual(len(pts), 16)
        for axis in range(2):
            self.assertAlmostEqual(max(p[axis] for p in pts) - min(p[axis] for p in pts), 0.076, delta=1e-12)
        area = sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(pts, pts[1:] + pts[:1])) / 2
        self.assertGreater(area, 0.0)   # 反時計回り
        for bad in ((0.076, 2), (0.0, 8)):
            with self.assertRaises(ValueError):
                pr.regular_polygon(*bad)

    def test_regular_octagon_has_equal_sides(self):
        pts = pr.regular_octagon(0.0335)
        sides = [math.dist(p, q) for p, q in zip(pts, pts[1:] + pts[:1])]
        self.assertTrue(all(abs(s - sides[0]) < 1e-12 for s in sides), sides)
        self.assertAlmostEqual(sum(sides), 0.111, delta=0.0005)   # 周 11.1 cm（4 3/8 in のグリップ）

    def test_rejects_bad_chamfer(self):
        for bad in (0.0, -0.001, 0.005, 0.01):
            with self.assertRaises(ValueError):
                pr.octagon(0.005, 0.0115, bad)


class TubeTest(unittest.TestCase):
    def test_capped_tube_faces_outward(self):
        section = pr.regular_octagon(0.02)
        verts, faces = pr.straight_tube((0.0, 0.0, 0.0), (0.3, 0.0, 0.0), section)
        area = 2 * 0.02 ** 2 * (math.sqrt(2) - 1)   # 正八角形の面積（対辺の距離 0.02）
        self.assertAlmostEqual(signed_volume(verts, faces), area * 0.3, delta=1e-12)
        self.assertEqual(len(faces), 8 + 2)

    def test_tube_in_any_direction_faces_outward(self):
        section = pr.regular_octagon(0.02)
        for end in ((0.0, 1.0, 0.0), (-1.0, 0.0, 0.0), (0.6, -0.8, 0.0)):
            with self.subTest(end=end):
                self.assertGreater(signed_volume(*pr.straight_tube((0.0, 0.0, 0.0), end, section)), 0.0)

    def test_rejects_bad_input(self):
        sq = [(1.0, 0.0), (0.0, 1.0), (-1.0, 0.0)]
        s = ((0.0, 0.0, 0.0), (0.0, 1.0), sq)
        with self.assertRaises(ValueError):
            pr.tube([s])
        with self.assertRaises(ValueError):
            pr.tube([s, s], closed=True, caps=True)
        with self.assertRaises(ValueError):
            pr.tube([s, ((1.0, 0.0, 0.0), (0.0, 1.0), sq[:2])])
        with self.assertRaises(ValueError):
            pr.straight_tube((0.0, 0.0, 0.0), (0.0, 0.0, 1.0), sq)   # 真上へは作らない（prism を使う）


class PrismBoxTest(unittest.TestCase):
    def test_prism(self):
        verts, faces = pr.prism((1.0, -2.0), pr.regular_octagon(0.06), 0.0, 3.05)
        lo, hi = bounds(verts)
        self.assertAlmostEqual(hi[0] - lo[0], 0.06, delta=TOL)
        self.assertAlmostEqual(lo[1] + hi[1], -4.0, delta=TOL)
        self.assertEqual((lo[2], hi[2]), (0.0, 3.05))
        area = 2 * 0.06 ** 2 * (math.sqrt(2) - 1)
        self.assertAlmostEqual(signed_volume(verts, faces), area * 3.05, delta=1e-12)
        with self.assertRaises(ValueError):
            pr.prism((0.0, 0.0), pr.regular_octagon(0.06), 1.0, 1.0)

    def test_box(self):
        verts, faces = pr.box((0.0, 2.0), (-0.5, 0.5), (0.0, 0.1))
        self.assertAlmostEqual(signed_volume(verts, faces), 0.2, delta=1e-12)


class JoinTest(unittest.TestCase):
    def test_merge_slots_offsets_indices(self):
        a = ([(0.0, 0.0, 0.0)] * 3, [(0, 1, 2)])
        verts, faces, names, indices = pr.merge_slots([("A", a), ("B", a), ("A", a)])
        self.assertEqual(len(verts), 9)
        self.assertEqual(faces, [(0, 1, 2), (3, 4, 5), (6, 7, 8)])
        self.assertEqual((names, indices), (["A", "B"], [0, 1, 0]))


if __name__ == "__main__":
    unittest.main()
