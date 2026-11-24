"""stadium_a.py：外形と期待値、面の向き、対称、席の数、段の傾き、カメラと審判台の場所、LOD と予算。"""
import math
import sys
import unittest
from pathlib import Path

sys.path[:0] = [str(Path(__file__).resolve().parents[1]), str(Path(__file__).resolve().parent)]

from geometry_helpers import bounds, signed_volume  # noqa: E402
from tennis_blender import dims as d  # noqa: E402
from tennis_blender import expected as ex  # noqa: E402
from tennis_blender import materials as mat  # noqa: E402
from tennis_blender import primitives as pr  # noqa: E402
from tennis_blender import stadium_a as sa  # noqa: E402

TOL = 1e-9
SEATED_HEAD = 1.2  # 座った観客の頭の上の高さ（床から。推定）


def _merged(items):
    return pr.merge_slots(list(items))


def _triangles(name) -> int:
    return sum(len(f) - 2 for f in _merged(sa.parts()[name])[1])


class ShapeTest(unittest.TestCase):
    def test_sizes_literal(self):
        """大きさ（X・Y・Z、m）。数値は ART-01 §4.3 の表と同じ。"""
        sizes = {"SM_Stadium_A_Stands": (102.23, 82.11, 16.5), "SM_Stadium_A_Stands_LOD1": (102.23, 82.11, 16.5),
                 "SM_Stadium_A_Barrier": (40.63, 20.51, 1.0), "SM_Stadium_A_Floor": (102.23, 82.11, 0.0)}
        self.assertEqual(set(sa.parts()), set(sizes))
        for name, items in sa.parts().items():
            lo, hi = bounds(_merged(items)[0])
            for axis in range(3):
                self.assertAlmostEqual(hi[axis] - lo[axis], sizes[name][axis], delta=1e-6, msg=name)

    def test_generated_parts_match_expected_bounds(self):
        for name, items in sa.parts().items():
            lo, hi = bounds(_merged(items)[0])
            for axis in range(3):
                self.assertAlmostEqual(lo[axis], ex.BOUNDS[name][0][axis], delta=TOL, msg=name)
                self.assertAlmostEqual(hi[axis], ex.BOUNDS[name][1][axis], delta=TOL, msg=name)

    def test_mirror_symmetric(self):
        """XZ・YZ の面について対称（FBX で Y が反転しても配置が変わらない）。"""
        for name, items in sa.parts().items():
            with self.subTest(name=name):
                keys = {tuple(round(c, 6) + 0.0 for c in v) for v in _merged(items)[0]}
                self.assertEqual(keys, {(round(-x, 6) + 0.0, y, z) for x, y, z in keys})
                self.assertEqual(keys, {(x, round(-y, 6) + 0.0, z) for x, y, z in keys})

    def test_every_piece_faces_outward(self):
        """部品ごとに体積が正（面が外を向く）。地面に接する面は作らないが、z = 0 の面は体積に効かない。"""
        pieces = sa.lower_tier(0) + sa.lower_tier(1) + [sa.upper_tier(0), sa.upper_tier(1)] + sa.barrier_pieces()
        for piece in pieces:
            self.assertGreater(signed_volume(*_merged(piece.items())[:2]), 0.0)
        for slot, (verts, faces) in sa.floor():   # 床は上を向く（xy の面で反時計回り）
            for f in faces:
                (ax, ay, _), (bx, by, _), (cx, cy, _) = (verts[i] for i in f[:3])
                self.assertGreater((bx - ax) * (cy - by) - (by - ay) * (cx - bx), 0.0, slot)

    def test_slots_have_preview_colors_and_lod1_is_a_prefix(self):
        for name, items in sa.parts().items():
            for slot, _ in items:
                self.assertIn(slot, mat.PREVIEW_COLORS, (name, slot))
        lod0 = [s for s, _ in sa.parts()["SM_Stadium_A_Stands"]]
        lod1 = [s for s, _ in sa.parts()["SM_Stadium_A_Stands_LOD1"]]
        self.assertEqual(lod0, [mat.STADIUM_CONCRETE, mat.STADIUM_SEAT, mat.STADIUM_AISLE])
        self.assertEqual(lod1, lod0[:len(lod1)])   # 同じスロットの番号が同じマテリアルになる

    def test_triangle_budgets(self):
        for label, (names, limit) in ex.TRI_BUDGETS.items():
            if label.startswith("スタジアム A"):
                total = sum(_triangles(n) for n in names)
                self.assertTrue(0 < total <= limit, (label, total))
        self.assertLess(_triangles("SM_Stadium_A_Stands_LOD1"), _triangles("SM_Stadium_A_Stands") / 10)


class LayoutTest(unittest.TestCase):
    def test_runoff_is_the_stadium_recommendation(self):
        self.assertAlmostEqual(d.STADIUM_HALF_X, 20.115, delta=TOL)   # 23.77 / 2 + 8.23
        self.assertAlmostEqual(d.STADIUM_HALF_Y, 10.055, delta=TOL)   # 10.97 / 2 + 4.57
        self.assertGreater(d.STADIUM_RUNOFF_BACK, d.RUNOFF_BACK)
        self.assertGreater(d.STADIUM_RUNOFF_SIDE, d.RUNOFF_SIDE)

    def test_about_15000_seats(self):
        """ART-01 A-STD-010：観客 15,000。"""
        self.assertTrue(14500 <= sa.seat_estimate() <= 15500, sa.seat_estimate())

    def test_rows_and_rake(self):
        """列の奥行き 0.8 m・席の幅 0.5 m 以上（FIFA）。傾きは 35° 以下（Green Guide 12.10。SGSA SG01 の引用）。"""
        self.assertGreaterEqual(d.STADIUM_ROW_DEPTH, 0.80)
        self.assertGreaterEqual(d.STADIUM_SEAT_WIDTH, 0.50)
        for riser in (d.STADIUM_LOWER_RISER, d.STADIUM_UPPER_RISER):
            self.assertLessEqual(math.degrees(math.atan2(riser, d.STADIUM_ROW_DEPTH)), 35.0)
        self.assertLess(d.STADIUM_LOWER_RISER, d.STADIUM_UPPER_RISER)   # 後ろの層ほど急にして、前の頭の上から見る

    def test_cameras_are_above_the_seated_heads(self):
        """中継カメラ（DD-07 §1.1：ベースラインの 15 m 後ろ・高さ 6 m）と、2 人対戦の横のカメラ（§1.2：ネットの横 18 m・
        高さ 9 m）は、下の層の上にあり、その列に座る観客の頭より上にある。"""
        for distance, wall, height in ((d.HALF_LENGTH + 15.0, d.STADIUM_HALF_X, 6.0), (18.0, d.STADIUM_HALF_Y, 9.0)):
            row = int((distance - wall - sa.LOWER_OFFSET) // d.STADIUM_ROW_DEPTH)
            self.assertTrue(0 <= row < d.STADIUM_LOWER_ROWS, row)
            self.assertLess(d.STADIUM_LOWER_FRONT + row * d.STADIUM_LOWER_RISER + SEATED_HEAD, height)

    def test_room_for_umpire_chair_and_scoreboard(self):
        """審判台（ART-01 §4.2：y = ±7.299 m、奥行き 0.9 m）の後ろに 2 m 以上。掲示板（§4.3：横の看板の壁の前、
        y = ±9.94 m）は選手の移動の範囲（DD-03 §2.4：横 ±8.99 m）の外で、カメラと選手の間に入らない。"""
        self.assertGreaterEqual(d.STADIUM_HALF_Y - (d.POST_Y + 0.9 + d.UMPIRE_CHAIR_DEPTH / 2), 2.0)
        board_y, board_x = 9.94, 16.9
        self.assertGreater(board_y - d.SCOREBOARD_PANEL[0] / 2 - d.SCOREBOARD_SCREEN_DEPTH, 8.99)
        self.assertLess(board_y + d.SCOREBOARD_PANEL[0] / 2, d.STADIUM_HALF_Y)
        corner_cut = d.STADIUM_HALF_X - d.STADIUM_CORNER_GAP / math.sqrt(2.0)   # 横の看板の壁の端（内側の面）
        self.assertTrue(d.HALF_LENGTH < board_x - d.SCOREBOARD_PANEL[1] / 2 and board_x + d.SCOREBOARD_PANEL[1] / 2 < corner_cut)

    def test_corner_gaps_are_open_and_aligned(self):
        """看板の壁と下の層は、四隅の対角線の向きの同じ幅（出入口）だけ開く。上の層は出入口の上をまたぐ（天井）。"""
        c = d.STADIUM_CORNER_GAP / math.sqrt(2.0)          # 辺の向きに測った切りの量
        lo, hi = d.STADIUM_HALF_X - d.STADIUM_HALF_Y - c, d.STADIUM_HALF_X - d.STADIUM_HALF_Y + c   # x − y の範囲
        diffs = [x - y for piece in sa.lower_tier(0) + sa.barrier_pieces()
                 for x, y, _ in _merged(piece.items())[0] if x > 0.0 and y > 0.0]
        self.assertFalse([v for v in diffs if lo + 1e-6 < v < hi - 1e-6])
        self.assertTrue(any(abs(v - lo) < 1e-6 for v in diffs) and any(abs(v - hi) < 1e-6 for v in diffs))
        ceiling = [z for x, y, z in _merged(sa.upper_tier(0).items())[0] if x > 0.0 and y > 0.0 and lo < x - y < hi]
        self.assertAlmostEqual(min(ceiling), sa.WALKWAY_Z, delta=TOL)

if __name__ == "__main__":
    unittest.main()
