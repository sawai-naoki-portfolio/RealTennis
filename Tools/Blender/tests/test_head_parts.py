"""head_parts.py：髪の生え際、髪・耳・目・眉・ポニーテール・ヘッドバンドの位置と面の向き。"""
import math
import sys
import unittest
from pathlib import Path

sys.path[:0] = [str(Path(__file__).resolve().parents[1]), str(Path(__file__).resolve().parent)]

from geometry_helpers import bounds, signed_volume  # noqa: E402
from tennis_blender import head_parts as hp  # noqa: E402
from tennis_blender import head_shape as hs  # noqa: E402


class HairlineTest(unittest.TestCase):
    def test_hairline_is_symmetric_and_flat_at_front_and_back(self):
        for style in (hp.SHORT, hp.TIED):
            for th in (0.3, 1.0, 2.0, 2.9):
                self.assertAlmostEqual(hp.hairline_z(style, th), hp.hairline_z(style, -th))
            for th in (0.0, math.pi):  # 傾きが 0（後ろがとがらない）
                self.assertAlmostEqual(hp.hairline_z(style, th + 0.02), hp.hairline_z(style, th - 0.02))
                self.assertAlmostEqual(hp.hairline_z(style, th), style.hairline[0 if th == 0.0 else 4])

    def test_thickness_grows_from_the_edge(self):
        style = hp.SHORT
        self.assertAlmostEqual(hp.hair_thickness(style, 0.0), style.edge)
        self.assertAlmostEqual(hp.hair_thickness(style, 1.0), style.top)
        values = [hp.hair_thickness(style, s / 20) for s in range(21)]
        self.assertEqual(values, sorted(values))


class PartsTest(unittest.TestCase):
    def test_hair_covers_the_crown_and_stays_above_the_hairline(self):
        verts, faces = hp.hair_mesh(hp.SHORT)
        self.assertGreater(max(v[2] for v in verts), hs.CROWN_Z)
        self.assertGreater(min(v[2] for v in verts), hp.SHORT.hairline[4] - 0.005)
        self.assertEqual(len(faces), 64 * 17 + 64)

    def test_closed_parts_point_outward(self):
        eye, iris = hp.eye_meshes(1)
        tail, tie = hp.ponytail_mesh(hp.TIED)
        for name, (verts, faces) in {"eye": eye, "iris": iris, "band": hp.headband_mesh(hp.TIED),
                                     "sphere": hp.uv_sphere((0.0, 0.0, 0.0), 1.0)}.items():
            with self.subTest(part=name):
                self.assertGreater(signed_volume(verts, faces), 0.0)
        self.assertAlmostEqual(signed_volume(*hp.uv_sphere((0.0, 0.0, 0.0), 1.0, segments=48, rings=32)),
                               4 / 3 * math.pi, delta=0.05)
        self.assertGreater(signed_volume(*tail), 0.0)  # 根元は開いているが、ほぼ閉じた筒

    def test_eyes_sit_in_front_and_irises_look_forward(self):
        for side in (1, -1):
            (eye_v, _), (iris_v, _) = hp.eye_meshes(side)
            eye_c = [sum(v[k] for v in eye_v) / len(eye_v) for k in range(3)]
            iris_c = [sum(v[k] for v in iris_v) / len(iris_v) for k in range(3)]
            self.assertAlmostEqual(eye_c[0], side * hs.EYE_X, places=6)
            self.assertLess(iris_c[1], eye_c[1])  # 黒目は白目の前（−Y）
            self.assertLess(eye_c[1], -0.07)

    def test_ears_are_on_the_sides_and_mirrored(self):
        left, right = hp.ear_mesh(1), hp.ear_mesh(-1)
        (llo, lhi), (rlo, rhi) = bounds(left[0]), bounds(right[0])
        self.assertGreater(llo[0], 0.06)
        self.assertLess(rhi[0], -0.06)
        self.assertAlmostEqual(lhi[0], -rlo[0], places=9)
        self.assertTrue(0.07 < llo[2] < lhi[2] < 0.16)

    def test_brows_face_forward_on_both_sides(self):
        for side in (1, -1):
            verts, faces = hp.brow_mesh(side)
            a, b, c = (verts[i] for i in faces[0][:3])  # 最初の面は前の面
            n_y = (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2])
            self.assertLess(n_y, 0.0)
            self.assertTrue(all(side * v[0] > 0.0 for v in verts))

    def test_ponytail_hangs_behind_the_head(self):
        (tail, _), _ = hp.ponytail_mesh(hp.TIED)
        back = hs.surface_point(math.pi, 0.15, features=False)[1]
        self.assertGreater(max(v[1] for v in tail), back + 0.02)
        self.assertLess(min(v[2] for v in tail), 0.15 - 0.18)


if __name__ == "__main__":
    unittest.main()
