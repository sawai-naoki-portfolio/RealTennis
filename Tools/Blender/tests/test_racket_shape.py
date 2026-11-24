"""racket_shape.py：ヘッド・スロート・グリップ・ストリングの形、面の向き、対称、予算、ソケット。"""
import sys
import unittest
from pathlib import Path

sys.path[:0] = [str(Path(__file__).resolve().parents[1]), str(Path(__file__).resolve().parent)]

from geometry_helpers import bounds, signed_volume  # noqa: E402
from tennis_blender import dims as d  # noqa: E402
from tennis_blender import materials as mat  # noqa: E402
from tennis_blender import primitives as pr  # noqa: E402
from tennis_blender import racket_shape as rs  # noqa: E402

TOL = 1e-9


def face_value(x, y, a=rs.FACE_A, b=rs.FACE_B):
    """打球面の楕円の式の値（1 未満なら打球面の内側）。"""
    return ((x - rs.FACE_CENTER_X) / a) ** 2 + (y / b) ** 2


class HeadTest(unittest.TestCase):
    def setUp(self):
        self.verts, self.faces = rs.head()

    def test_closed_and_outward(self):
        self.assertEqual(len(self.faces), d.RACKET_RING_SEGMENTS * 8)
        self.assertGreater(signed_volume(self.verts, self.faces), 0.0)

    def test_outer_size_and_tip(self):
        lo, hi = bounds(self.verts)
        self.assertAlmostEqual(hi[0], d.RACKET_LENGTH, delta=TOL)
        self.assertAlmostEqual(hi[0] - lo[0], d.RACKET_FACE_LENGTH + 2 * d.RACKET_FRAME_WIDTH, delta=TOL)
        self.assertAlmostEqual(hi[1], d.RACKET_FACE_WIDTH / 2 + d.RACKET_FRAME_WIDTH, delta=TOL)
        self.assertAlmostEqual(hi[2], d.RACKET_BEAM / 2, delta=TOL)

    def test_inner_edge_is_the_face(self):
        """フレームは打球面の外にあり、内側の縁が打球面の楕円に乗る。"""
        values = [face_value(x, y) for x, y, _ in self.verts]
        self.assertGreaterEqual(min(values), 1.0 - 1e-9)
        self.assertAlmostEqual(min(values), 1.0, delta=1e-9)


class ThroatGripTest(unittest.TestCase):
    def test_throat_closed_and_reaches_frame(self):
        verts, faces = rs.throat()
        self.assertGreater(signed_volume(verts, faces), 0.0)
        lo, hi = bounds(verts)
        self.assertLess(lo[0], d.RACKET_THROAT_X)                    # 根元はシャフトの中
        self.assertLess(hi[2], d.RACKET_BEAM / 2)                    # フレームより薄い（面が重ならない）
        head_bottom = d.RACKET_LENGTH - d.RACKET_FACE_LENGTH - 2 * d.RACKET_FRAME_WIDTH
        self.assertGreater(hi[0], head_bottom)

    def test_grip_and_shaft_form_a_closed_handle(self):
        (gv, gf), (sv, sf) = rs.grip(), rs.shaft()
        verts = gv + sv
        faces = gf + [tuple(len(gv) + i for i in f) for f in sf]
        self.assertGreater(signed_volume(verts, faces), 0.0)
        lo, hi = bounds(verts)
        self.assertAlmostEqual(lo[0], 0.0, delta=TOL)                # 原点＝グリップエンド
        self.assertAlmostEqual(hi[0], d.RACKET_THROAT_X, delta=TOL)
        self.assertAlmostEqual(hi[2], d.RACKET_BUTT_ACROSS / 2, delta=TOL)
        self.assertEqual(gv[-8:], sv[:8])                            # グリップの端とシャフトの根元が同じ断面


class StringsTest(unittest.TestCase):
    def test_counts_and_spacing(self):
        verts, faces = rs.strings()
        self.assertEqual(len(faces), 6 * (d.RACKET_STRING_MAINS + d.RACKET_STRING_CROSSES))
        self.assertGreater(signed_volume(verts, faces), 0.0)
        ys = rs.main_ys()
        self.assertEqual(len(ys), 16)
        self.assertAlmostEqual(ys[0], -ys[-1], delta=TOL)
        self.assertEqual(len(rs.cross_xs()), 19)

    def test_ends_are_inside_the_frame(self):
        """端（すべての頂点）が打球面の外、ヘッドの外側の縁より内にある（隙間も突き出しもない）。"""
        verts, _ = rs.strings()
        outer_a, outer_b = rs.FACE_A + d.RACKET_FRAME_WIDTH, rs.FACE_B + d.RACKET_FRAME_WIDTH
        for x, y, _ in verts:
            self.assertGreater(face_value(x, y), 1.0)
            self.assertLess(face_value(x, y, outer_a, outer_b), 1.0)

    def test_crosses_are_thinner_than_mains(self):
        verts, _ = rs.strings()
        mains = verts[:8 * d.RACKET_STRING_MAINS]
        crosses = verts[8 * d.RACKET_STRING_MAINS:]
        self.assertAlmostEqual(max(v[2] for v in mains), d.RACKET_STRING_GAUGE / 2, delta=TOL)
        self.assertLess(max(v[2] for v in crosses), max(v[2] for v in mains))


class RacketTest(unittest.TestCase):
    def test_mirror_symmetric(self):
        """XZ・XY の面について対称（FBX で Y が反転しても形が変わらない）。"""
        verts, _, _, _ = pr.merge_slots(rs.parts())
        keys = {tuple(round(c, 9) for c in v) for v in verts}
        for x, y, z in verts:
            self.assertIn((round(x, 9), round(-y, 9), round(z, 9)), keys)
            self.assertIn((round(x, 9), round(y, 9), round(-z, 9)), keys)

    def test_slots_and_triangle_budget(self):
        verts, faces, names, indices = pr.merge_slots(rs.parts())
        self.assertEqual(names, [mat.RACKET_GRIP, mat.RACKET_FRAME, mat.RACKET_STRINGS])
        self.assertEqual(len(indices), len(faces))
        self.assertGreater(signed_volume(verts, faces), 0.0)
        triangles = sum(len(f) - 2 for f in faces)
        self.assertLessEqual(triangles, 6000)                        # ART-01 §1（4〜6k）

    def test_sockets(self):
        s = rs.sockets()
        self.assertEqual(set(s), {"SOCKET_racket_sweetspot", "SOCKET_grip"})
        x, y, z = s["SOCKET_racket_sweetspot"]
        self.assertAlmostEqual(rs.FACE_CENTER_X - x, 0.035, delta=TOL)   # 打球面の中心から 3.5 cm グリップ寄り
        self.assertLess(face_value(x, y), 1.0)
        gx = s["SOCKET_grip"][0]
        self.assertTrue(d.RACKET_BUTT_LENGTH < gx < d.RACKET_GRIP_LENGTH)


if __name__ == "__main__":
    unittest.main()
