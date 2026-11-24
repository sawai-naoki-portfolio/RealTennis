"""practice_court.py：フェンスの支柱の間隔、面の向き、ベンチと照明柱の位置、対称、予算。"""
import math
import sys
import unittest
from pathlib import Path

sys.path[:0] = [str(Path(__file__).resolve().parents[1]), str(Path(__file__).resolve().parent)]

from geometry_helpers import bounds, signed_volume  # noqa: E402
from tennis_blender import dims as d  # noqa: E402
from tennis_blender import practice_court as pc  # noqa: E402
from tennis_blender import primitives as pr  # noqa: E402

TOL = 1e-9


class FenceTest(unittest.TestCase):
    def test_posts_on_runoff_edge_with_max_spacing(self):
        posts = pc.fence_posts_xy()
        self.assertEqual(len(posts), len(set(posts)))
        self.assertEqual(len(posts), 2 * 13 + 2 * 5)                 # 長い辺 12 区間、短い辺 6 区間
        for x, y in posts:
            self.assertTrue(abs(abs(x) - 18.285) < TOL or abs(abs(y) - 9.145) < TOL, (x, y))
        for half in (d.FENCE_HALF_X, d.FENCE_HALF_Y):
            ps = pc.side_positions(half)
            steps = [b - a for a, b in zip(ps, ps[1:])]
            self.assertLessEqual(max(steps), d.FENCE_POST_SPACING_MAX + TOL)
            self.assertAlmostEqual(max(steps), min(steps), delta=TOL)
            self.assertEqual((ps[0], ps[-1]), (-half, half))

    def test_closed_parts_face_outward(self):
        for mesh in (pc.fence_posts(), pc.fence_rails(), pc.benches(), *pc.light_poles()):
            self.assertGreater(signed_volume(*mesh), 0.0)

    def test_net_faces_the_court(self):
        verts, faces = pc.fence_net()
        self.assertEqual(len(faces), 4)
        for face in faces:
            a, b, c = (verts[i] for i in face[:3])
            u = [b[k] - a[k] for k in range(3)]
            v = [c[k] - b[k] for k in range(3)]
            n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
            center = [sum(verts[i][k] for i in face) / 4 for k in range(2)]
            self.assertLess(n[0] * center[0] + n[1] * center[1], 0.0)   # 法線は原点（コート）の方

    def test_rail_below_post_tops(self):
        _, hi = bounds(pc.fence_rails()[0])
        self.assertLess(hi[2], d.FENCE_HEIGHT)


class BenchLightTest(unittest.TestCase):
    def test_bench_between_net_post_and_fence(self):
        verts, _ = pc.benches()
        ys = [abs(y) for _, y, _ in verts]
        self.assertGreater(min(ys), d.POST_Y + d.POST_DIAMETER / 2)
        self.assertLess(max(ys), d.FENCE_HALF_Y - d.FENCE_POST_ACROSS / 2)
        self.assertAlmostEqual(max(z for *_, z in verts), d.BENCH_SEAT_HEIGHT, delta=TOL)

    def test_lights_outside_fence_and_heads_face_court(self):
        poles, heads = pc.light_poles()
        fence_out = d.FENCE_HALF_Y + d.FENCE_POST_ACROSS / 2
        pole_inner = d.LIGHT_POLE_Y - d.LIGHT_POLE_ACROSS / 2
        self.assertGreater(pole_inner, fence_out)
        self.assertTrue(all(abs(y) < pole_inner for _, y, _ in heads[0]))          # 灯具はコートの方へ出る
        self.assertAlmostEqual(bounds(heads[0])[1][2], d.LIGHT_POLE_HEIGHT, delta=TOL)
        self.assertEqual(len(heads[1]), 4 * 6)


class PracticeCourtTest(unittest.TestCase):
    def test_mirror_symmetric_and_budget(self):
        for name, items in pc.parts().items():
            with self.subTest(name=name):
                verts, faces, names, indices = pr.merge_slots(items)
                keys = {tuple(round(c, 9) for c in v) for v in verts}
                for x, y, z in verts:
                    self.assertIn((round(-x, 9), round(y, 9), round(z, 9)), keys)
                    self.assertIn((round(x, 9), round(-y, 9), round(z, 9)), keys)
                self.assertLessEqual(sum(len(f) - 2 for f in faces), 10000)   # ART-01 §1（小物 1〜10k）
                self.assertEqual(len(indices), len(faces))
                self.assertEqual(min(v[2] for v in verts), 0.0)             # 地面に立つ

    def test_fence_height_is_10_ft(self):
        self.assertAlmostEqual(d.FENCE_HEIGHT, 10 * 0.3048, delta=0.005)   # 10 ft（許容 5 mm）
        self.assertTrue(math.isclose(d.FENCE_HALF_X, 18.285) and math.isclose(d.FENCE_HALF_Y, 9.145))


if __name__ == "__main__":
    unittest.main()
