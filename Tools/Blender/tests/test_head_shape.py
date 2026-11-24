"""head_shape.py：頭と首の形（開いているのは首の下だけ、左右対称、面は外向き、寸法、顔の位置、ウェイト）。"""
import math
import sys
import unittest
from pathlib import Path

sys.path[:0] = [str(Path(__file__).resolve().parents[1]), str(Path(__file__).resolve().parent)]

from geometry_helpers import bounds  # noqa: E402
from tennis_blender import head_shape as hs  # noqa: E402

SEGMENTS = 72


def edge_use(faces):
    use = {}
    for f in faces:
        for i, a in enumerate(f):
            e = (min(a, f[(i + 1) % len(f)]), max(a, f[(i + 1) % len(f)]))
            use[e] = use.get(e, 0) + 1
    return use


def normal(verts, face):
    a, b, c = (verts[i] for i in face[:3])
    u = [b[k] - a[k] for k in range(3)]
    v = [c[k] - a[k] for k in range(3)]
    return u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]


class HeadMeshTest(unittest.TestCase):
    def setUp(self):
        self.verts, self.faces, self.lips = hs.head_mesh(hs.HeadDims(), SEGMENTS)

    def test_only_the_neck_bottom_is_open(self):
        open_edges = [e for e, n in edge_use(self.faces).items() if n == 1]
        self.assertEqual(len(open_edges), SEGMENTS)
        self.assertTrue(all(self.verts[a][2] == self.verts[b][2] == hs.NECK_BOTTOM_Z for a, b in open_edges))
        self.assertTrue(all(n in (1, 2) for n in edge_use(self.faces).values()))

    def test_left_and_right_are_mirrors(self):
        key = {(round(x, 9), round(y, 9), round(z, 9)) for x, y, z in self.verts}
        self.assertTrue(all((round(-x, 9) or 0.0, round(y, 9), round(z, 9)) in key for x, y, z in self.verts))

    def test_faces_point_outward(self):
        """横の面は断面の中心から外へ、頭頂の面は上へ、あごの下の面は下へ向く。"""
        outward = 0
        for f in self.faces:
            c = [sum(self.verts[i][k] for i in f) / len(f) for k in range(3)]
            _, front, back, _, _ = hs.profile_at(c[2])
            n = normal(self.verts, f)
            radial = n[0] * c[0] + n[1] * (c[1] + (front - back) / 2)
            outward += radial > 0.0 or (c[2] > 0.2 and n[2] > 0.0) or (c[2] < 0.02 and n[2] < 0.0)
        self.assertGreater(outward / len(self.faces), 0.99)

    def test_size_of_an_adult_male_head(self):
        lo, hi = bounds(self.verts)
        self.assertAlmostEqual(hi[2], hs.CROWN_Z, places=9)
        head = [v for v in self.verts if v[2] >= 0.0]
        width = max(v[0] for v in head) - min(v[0] for v in head)
        self.assertTrue(0.150 <= width <= 0.165, width)

    def test_nose_tip_is_the_front_and_lips_are_on_the_face(self):
        tip = min((v for v in self.verts if v[2] >= 0.0), key=lambda v: v[1])  # 前は −Y
        self.assertTrue(0.07 <= tip[2] <= 0.09 and abs(tip[0]) < 1e-9, tip)
        lips = [self.faces[i] for i, lip in enumerate(self.lips) if lip]
        self.assertGreater(len(lips), 10)
        self.assertTrue(all(self.verts[i][1] < 0.0 for f in lips for i in f))

    def test_dims_scale_the_head(self):
        verts, _, _ = hs.head_mesh(hs.HeadDims(width=0.95, depth=0.95, height=0.94, neck=0.86), SEGMENTS)
        self.assertAlmostEqual(max(v[2] for v in verts), hs.CROWN_Z * 0.94, places=9)


class ProfileTest(unittest.TestCase):
    def test_pchip_hits_the_keys_and_does_not_overshoot(self):
        xs, ys = [0.0, 1.0, 2.0, 3.0], [0.0, 1.0, 1.0, 0.0]
        for x, y in zip(xs, ys):
            self.assertAlmostEqual(hs.pchip(xs, ys, x), y)
        self.assertTrue(all(0.0 <= hs.pchip(xs, ys, i / 10) <= 1.0 for i in range(31)))

    def test_theta_for_x_finds_the_eye(self):
        th = hs.theta_for_x(hs.EYE_X, hs.EYE_Z)
        x, _, _ = hs.surface_point(th, hs.EYE_Z, features=False)
        self.assertAlmostEqual(x, hs.EYE_X, places=6)
        self.assertAlmostEqual(hs.theta_for_x(-hs.EYE_X, hs.EYE_Z), -th, places=9)

    def test_face_features_only_change_the_front(self):
        for z in (0.05, 0.08, 0.12):
            back = hs.surface_point(math.pi, z), hs.surface_point(math.pi, z, features=False)
            self.assertEqual(back[0], back[1])


class NeckWeightsTest(unittest.TestCase):
    def test_weights_blend_from_the_spine_to_the_head(self):
        self.assertEqual(hs.neck_weights(0.05), [("head", 1.0)])
        self.assertEqual(hs.neck_weights(hs.NECK_BOTTOM_Z), [("spine_05", 1.0)])
        for z in (-0.1, -0.06, -0.02, -0.001):
            w = hs.neck_weights(z)
            self.assertAlmostEqual(sum(v for _, v in w), 1.0)
            self.assertTrue(all(0.0 < v <= 1.0 for _, v in w))


if __name__ == "__main__":
    unittest.main()
