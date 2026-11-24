"""mesh_ops.py：隣り合い・縮める・縁・平らにする（溝を埋め、下へ行かない）・ウェイトを混ぜる・縁の輪と筒。"""
import math
import sys
import unittest
from pathlib import Path

sys.path[:0] = [str(Path(__file__).resolve().parents[1]), str(Path(__file__).resolve().parent)]

from geometry_helpers import signed_volume  # noqa: E402
from tennis_blender import mesh_ops as mo  # noqa: E402


def grid(n):
    """n × n の頂点の、(n−1)² 個の四角形（z＝0 の平面）。"""
    verts = [(float(x), float(y), 0.0) for y in range(n) for x in range(n)]
    faces = [(y * n + x, y * n + x + 1, (y + 1) * n + x + 1, (y + 1) * n + x) for y in range(n - 1) for x in range(n - 1)]
    return verts, faces


class MeshOpsTest(unittest.TestCase):
    def test_erode_and_border(self):
        verts, faces = grid(5)
        adj = mo.adjacency(len(verts), faces)
        inner = [1 <= i % 5 <= 3 and 1 <= i // 5 <= 3 for i in range(25)]  # 真ん中の 3 × 3
        self.assertEqual([i for i, k in enumerate(mo.erode(inner, adj, 1)) if k], [12])
        border = mo.border_neighbors(faces, [True] * len(faces))
        self.assertEqual(len(border), 16)
        self.assertTrue(all(len(nb) == 2 for nb in border.values()))
        loops = mo.boundary_loops(faces, [True] * len(faces))
        self.assertEqual([len(lp) for lp in loops], [16])

    def test_region_adjacency_keeps_border_vertices_on_the_border(self):
        verts, faces = grid(4)
        adj = mo.adjacency(len(verts), faces)
        region = mo.region_adjacency(adj, [True] * 16, faces)
        self.assertEqual(sorted(region[1]), [0, 2])     # 縁：縁の隣だけ
        self.assertEqual(sorted(region[5]), [1, 4, 6, 9])  # 内：すべての隣

    def test_drape_fills_grooves_but_does_not_sink(self):
        verts, faces = grid(5)
        verts[12] = (2.0, 2.0, -0.5)  # 真ん中の溝
        normals = [(0.0, 0.0, 1.0)] * 25
        mask = [True] * 25
        out = mo.drape(verts, normals, faces, mo.adjacency(25, faces), mask, [0.1] * 25, steps=20)
        self.assertGreater(out[12][2], -0.2)                    # 溝が持ち上がる
        self.assertTrue(all(o[2] >= v[2] + 0.1 - 1e-9 for o, v in zip(out, verts)))  # 元から 0.1 より下へ行かない

    def test_relax_moves_only_movable_vertices(self):
        verts, faces = grid(3)
        verts[4] = (1.0, 1.0, 1.0)
        adj = mo.adjacency(9, faces)
        self.assertEqual(mo.relax(verts, adj, [False] * 9), verts)
        self.assertAlmostEqual(mo.relax(verts, adj, [i == 4 for i in range(9)], 1.0)[4][2], 0.0)

    def test_blend_weights(self):
        w = dict(mo.blend_weights([(0.001, [("a", 1.0)]), (0.1, [("b", 1.0)]), (0.1, [("c", 0.5), ("d", 0.5)])], 2))
        self.assertEqual(set(w), {"a", "b"})
        self.assertAlmostEqual(sum(w.values()), 1.0)
        self.assertGreater(w["a"], 0.9)
        with self.assertRaises(ValueError):
            mo.blend_weights([(0.1, [])])


class LoopTest(unittest.TestCase):
    def test_smooth_and_resample_a_square(self):
        square = [(1.0, 0.0, 0.0), (1.0, 1.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 0.0)]
        smooth = mo.smooth_loop(square, 3)
        self.assertAlmostEqual(sum(p[0] for p in smooth) / 4, 0.5)
        ring = mo.resample_loop(square, 8)
        self.assertEqual(len(ring), 8)
        gaps = [math.dist(ring[i], ring[(i + 1) % 8]) for i in range(8)]
        self.assertAlmostEqual(min(gaps), max(gaps))

    def test_collar_tube_points_outward(self):
        circle = [(0.08 * math.cos(2 * math.pi * i / 36), 0.08 * math.sin(2 * math.pi * i / 36), 1.5) for i in range(36)]
        verts, faces = mo.loop_tube(circle, (0.0, 0.0, 0.0), 0.008)
        self.assertEqual(len(verts), 36 * 8)
        self.assertGreater(signed_volume(verts, faces), 0.0)


if __name__ == "__main__":
    unittest.main()
