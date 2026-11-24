"""expected.py：検証の期待値が ITF の寸法どおりで、比較の関数が許容 5 mm で判定すること。"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tennis_blender import expected as ex  # noqa: E402
from tennis_blender import layout  # noqa: E402
from tennis_blender import practice_court  # noqa: E402
from tennis_blender import primitives  # noqa: E402
from tennis_blender import racket_shape  # noqa: E402

TOL = 1e-9


def _size(name):
    lo, hi = ex.BOUNDS[name]
    return tuple(h - l for l, h in zip(lo, hi))


class CourtBoundsTest(unittest.TestCase):
    def test_court_sizes(self):
        for name, (sx, sy) in {"SM_Court_Surface": (36.57, 18.29),
                               "SM_Court_Inner": (23.77, 10.97),
                               "SM_Court_Lines": (23.77, 10.97)}.items():
            size = _size(name)
            self.assertAlmostEqual(size[0], sx, delta=TOL, msg=name)
            self.assertAlmostEqual(size[1], sy, delta=TOL, msg=name)

    def test_court_on_ground_centered(self):
        for name in ("SM_Court_Surface", "SM_Court_Inner", "SM_Court_Lines"):
            lo, hi = ex.BOUNDS[name]
            self.assertEqual((lo[2], hi[2]), (0.0, 0.0), name)
            self.assertAlmostEqual(lo[0] + hi[0], 0.0, delta=TOL, msg=name)


class NetBoundsTest(unittest.TestCase):
    def test_net_parts(self):
        lo, hi = ex.BOUNDS["SM_NetPost"]
        self.assertAlmostEqual(hi[0] - lo[0], 0.15, delta=TOL)            # 直径 15 cm
        self.assertAlmostEqual(hi[1], 6.399 + 0.075, delta=TOL)           # 中心 y = 6.399
        self.assertAlmostEqual(hi[2], 1.095, delta=TOL)
        lo, hi = ex.BOUNDS["SM_SinglesStick"]
        self.assertAlmostEqual(hi[0] - lo[0], 0.075, delta=TOL)
        self.assertAlmostEqual(hi[1], 5.029 + 0.0375, delta=TOL)
        self.assertAlmostEqual(hi[2], 1.07, delta=TOL)
        lo, hi = ex.BOUNDS["SM_Net"]
        self.assertEqual((lo[0], hi[0], lo[2]), (0.0, 0.0, 0.0))
        self.assertAlmostEqual(hi[1], 6.399, delta=TOL)
        self.assertAlmostEqual(hi[2], 1.07, delta=TOL)

    def test_net_top_heights(self):
        heights = dict(ex.NET_TOP_HEIGHTS)
        self.assertAlmostEqual(heights[0.0], 0.914, delta=TOL)
        self.assertEqual(len(heights), 5)
        self.assertTrue(all(abs(h - 1.07) < TOL for y, h in heights.items() if y != 0.0))


class BallBoundsTest(unittest.TestCase):
    def test_ball_sizes_centered(self):
        for name, diameter in {"SM_Ball": 0.067, "SM_Ball_LOD1": 0.067, "SM_Ball_Large": 0.0715}.items():
            lo, hi = ex.BOUNDS[name]
            for axis in range(3):
                self.assertAlmostEqual(hi[axis] - lo[axis], diameter, delta=TOL, msg=name)
                self.assertAlmostEqual(hi[axis] + lo[axis], 0.0, delta=TOL, msg=name)   # 原点は中心


class RacketExpectedTest(unittest.TestCase):
    def test_racket_bounds_literal(self):
        lo, hi = ex.BOUNDS["SM_Racket"]
        self.assertEqual(lo[0], 0.0)                                       # 原点＝グリップエンド
        for got, want in zip(_size("SM_Racket"), (0.686, 0.27, 0.037)):
            self.assertAlmostEqual(got, want, delta=TOL)
        self.assertTrue(all(abs(a + b) < TOL for a, b in zip(lo[1:], hi[1:])))   # Y・Z は対称

    def test_sockets_literal(self):
        for name, x in {"SOCKET_racket_sweetspot": 0.481, "SOCKET_grip": 0.06}.items():
            parent, loc = ex.SOCKETS[name]
            self.assertEqual(parent, "SM_Racket")
            for got, want in zip(loc, (x, 0.0, 0.0)):
                self.assertAlmostEqual(got, want, delta=TOL, msg=name)

    def test_generated_racket_matches(self):
        """生成に使う racket_shape の頂点の範囲とソケットが、独立に求めた期待値と一致する。"""
        verts, _, _, _ = primitives.merge_slots(racket_shape.parts())
        lo, hi = ex.BOUNDS["SM_Racket"]
        for axis in range(3):
            self.assertAlmostEqual(min(v[axis] for v in verts), lo[axis], delta=TOL)
            self.assertAlmostEqual(max(v[axis] for v in verts), hi[axis], delta=TOL)
        sockets = racket_shape.sockets()
        expected = {name: loc for name, (parent, loc) in ex.SOCKETS.items() if parent == "SM_Racket"}
        self.assertEqual(set(sockets), set(expected))
        for name, loc in expected.items():
            for got, want in zip(sockets[name], loc):
                self.assertAlmostEqual(got, want, delta=TOL, msg=name)


class PracticeExpectedTest(unittest.TestCase):
    def test_practice_bounds_literal(self):
        for name, size in {"SM_Practice_Fence": (36.63, 18.35, 3.05), "SM_Practice_Bench": (1.80, 16.99, 0.45),
                           "SM_Practice_LightPole": (13.40, 19.44, 8.0)}.items():
            for got, want in zip(_size(name), size):
                self.assertAlmostEqual(got, want, delta=TOL, msg=name)

    def test_generated_practice_matches(self):
        """生成に使う practice_court の頂点の範囲が、独立に求めた期待値と一致する。"""
        for name, items in practice_court.parts().items():
            verts, _, _, _ = primitives.merge_slots(items)
            lo, hi = ex.BOUNDS[name]
            for axis in range(3):
                self.assertAlmostEqual(min(v[axis] for v in verts), lo[axis], delta=TOL, msg=name)
                self.assertAlmostEqual(max(v[axis] for v in verts), hi[axis], delta=TOL, msg=name)


class FaceCountsTest(unittest.TestCase):
    def test_face_counts(self):
        self.assertEqual(ex.FACE_COUNTS, {"SM_Net": 64 * 12, "SM_Ball": 32 * 16,
                                          "SM_Ball_LOD1": 16 * 8, "SM_Ball_Large": 32 * 16})


class CourtLineEdgesTest(unittest.TestCase):
    def test_edges_literal(self):
        xs, ys = ex.court_line_edges()
        for v in (11.885, 11.785, 11.685, 6.40, 6.35):
            self.assertTrue(any(abs(x - v) < TOL for x in xs), v)
            self.assertTrue(any(abs(x + v) < TOL for x in xs), -v)
        for v in (5.485, 5.435, 4.115, 4.065, 0.025):
            self.assertTrue(any(abs(y - v) < TOL for y in ys), v)
        self.assertEqual((len(xs), len(ys)), (10, 10))

    def test_layout_vertices_match_edges(self):
        """生成に使う layout の頂点が、独立に求めた縁の座標と過不足なく一致する。"""
        verts, _ = layout.rects_to_quads(layout.court_lines())
        xs = sorted({round(x, 6) for x, _ in verts})
        ys = sorted({round(y, 6) for _, y in verts})
        exp_x, exp_y = ex.court_line_edges()
        self.assertEqual(ex.unmatched(xs, exp_x, TOL) + ex.unmatched(ys, exp_y, TOL), [])
        self.assertEqual(ex.unmatched(exp_x, xs, TOL) + ex.unmatched(exp_y, ys, TOL), [])


class CompareTest(unittest.TestCase):
    def test_within_boundary(self):
        self.assertTrue(ex.within(1.0045, 1.0))
        self.assertTrue(ex.within(0.9955, 1.0))
        self.assertFalse(ex.within(1.0051, 1.0))
        self.assertFalse(ex.within(0.9949, 1.0))

    def test_unmatched(self):
        self.assertEqual(ex.unmatched([1.0, 2.003, 3.1], [1.0, 2.0, 3.0]), [3.1])
        self.assertEqual(ex.unmatched([], [1.0]), [])


if __name__ == "__main__":
    unittest.main()
