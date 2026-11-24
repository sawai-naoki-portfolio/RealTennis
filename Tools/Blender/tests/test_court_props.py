"""court_props.py：審判台・球出し機・ボールの缶・クーラーボックス・的・掲示板の寸法、面の向き、対称、ソケット、予算。"""
import sys
import unittest
from pathlib import Path

sys.path[:0] = [str(Path(__file__).resolve().parents[1]), str(Path(__file__).resolve().parent)]

from geometry_helpers import bounds, signed_volume  # noqa: E402
from tennis_blender import court_props as cp  # noqa: E402
from tennis_blender import dims as d  # noqa: E402
from tennis_blender import expected as ex  # noqa: E402
from tennis_blender import materials as mat  # noqa: E402
from tennis_blender import primitives as pr  # noqa: E402

TOL = 1e-9


def _triangles(faces) -> int:
    return sum(len(face) - 2 for face in faces)


class PartsTest(unittest.TestCase):
    def test_sizes_literal(self):
        """大きさ（長さ X・幅 Y・高さ Z、m）。数値は ART-01 §4.2 の表と同じ。"""
        sizes = {"SM_Prop_UmpireChair": (0.90, 0.70, 2.40), "SM_Prop_BallMachine": (0.56, 0.48, 0.76),
                 "SM_Prop_BallCan": (0.078, 0.078, 0.216), "SM_Prop_Cooler": (0.66, 0.38, 0.40),
                 "SM_Prop_ServeTarget": (1.00, 1.00, 0.005), "SM_Prop_Scoreboard": (0.155, 2.40, 2.20)}
        self.assertEqual(set(cp.parts()), set(sizes))
        for name, items in cp.parts().items():
            lo, hi = bounds(pr.merge_slots(items)[0])
            for axis in range(3):
                self.assertAlmostEqual(hi[axis] - lo[axis], sizes[name][axis], delta=TOL, msg=name)

    def test_generated_parts_match_expected_bounds(self):
        """生成した頂点の範囲が、dims から独立に求めた期待値（検査で使う）と一致する。"""
        for name, items in cp.parts().items():
            lo, hi = bounds(pr.merge_slots(items)[0])
            for axis in range(3):
                self.assertAlmostEqual(lo[axis], ex.BOUNDS[name][0][axis], delta=TOL, msg=name)
                self.assertAlmostEqual(hi[axis], ex.BOUNDS[name][1][axis], delta=TOL, msg=name)

    def test_stand_on_the_ground_and_are_symmetric_in_y(self):
        """原点は接地面。XZ の面について対称（FBX で Y が反転しても形が変わらない）。"""
        for name, items in cp.parts().items():
            verts = pr.merge_slots(items)[0]
            lo, hi = bounds(verts)
            self.assertAlmostEqual(lo[2], 0.0, delta=TOL, msg=name)
            self.assertAlmostEqual(lo[1] + hi[1], 0.0, delta=TOL, msg=name)
            points = {(round(x, 9), round(y, 9), round(z, 9)) for x, y, z in verts}
            self.assertEqual(points, {(x, -y if y else 0.0, z) for x, y, z in points}, name)

    def test_every_part_is_closed_and_faces_outward(self):
        for name, items in cp.parts().items():
            for slot, (verts, faces) in items:
                self.assertGreater(signed_volume(verts, faces), 0.0, (name, slot))

    def test_slots_have_preview_colors(self):
        for name, items in cp.parts().items():
            for slot, _ in items:
                self.assertIn(slot, mat.PREVIEW_COLORS, (name, slot))

    def test_triangles_within_the_prop_budget(self):
        names, limit = ex.TRI_BUDGETS["コートサイドの小物"]
        self.assertEqual(set(names), set(cp.parts()))
        total = sum(_triangles(pr.merge_slots(items)[1]) for items in cp.parts().values())
        self.assertTrue(0 < total <= limit, total)


class UmpireChairTest(unittest.TestCase):
    def test_seat_height_is_within_the_itf_recommendation(self):
        """ITF Duties and Procedures for Officials：審判台は 6 ft（1.82 m）以上、8 ft（2.44 m）以下。"""
        self.assertTrue(1.82 <= d.UMPIRE_CHAIR_SEAT_HEIGHT <= 2.44)
        _, (verts, _) = cp.umpire_chair()
        seat_top = max(z for x, _, z in verts if x > -d.UMPIRE_CHAIR_DEPTH / 2 + d.UMPIRE_CHAIR_BAR + TOL)
        self.assertAlmostEqual(seat_top, d.UMPIRE_CHAIR_SEAT_HEIGHT, delta=TOL)

    def test_rungs_are_evenly_spaced_and_climbable(self):
        step = d.UMPIRE_CHAIR_SEAT_HEIGHT / (d.UMPIRE_CHAIR_RUNGS + 1)
        self.assertTrue(0.25 <= step <= 0.35, step)   # はしごの段の間（一般に 0.25〜0.35 m）
        self.assertLess(d.UMPIRE_CHAIR_SEAT_HEIGHT - d.UMPIRE_CHAIR_FOOTREST_DROP, d.UMPIRE_CHAIR_SEAT_HEIGHT)


class BallMachineTest(unittest.TestCase):
    def test_muzzle_socket_is_the_front_most_point_on_the_center_line(self):
        sockets = cp.sockets()
        self.assertEqual(set(sockets), {"SOCKET_muzzle"})
        parent, (x, y, z) = sockets["SOCKET_muzzle"]
        self.assertEqual(parent, "SM_Prop_BallMachine")
        verts = pr.merge_slots(cp.parts()[parent])[0]
        self.assertAlmostEqual(x, max(v[0] for v in verts), delta=TOL)
        self.assertEqual(y, 0.0)
        self.assertAlmostEqual(z, d.BALL_MACHINE_MUZZLE_HEIGHT, delta=TOL)
        for got, want in zip((x, y, z), ex.SOCKETS["SOCKET_muzzle"][1]):
            self.assertAlmostEqual(got, want, delta=TOL)

    def test_ball_fits_the_muzzle_and_wheels_touch_the_ground(self):
        self.assertGreater(d.BALL_MACHINE_MUZZLE_ACROSS, 2 * d.BALL_RADIUS_LARGE)   # いちばん大きい球が通る
        half = d.BALL_MACHINE_MUZZLE_ACROSS / 2
        body_z = (d.BALL_MACHINE_CLEARANCE, d.BALL_MACHINE_CLEARANCE + d.BALL_MACHINE_BODY[2])
        self.assertTrue(body_z[0] <= d.BALL_MACHINE_MUZZLE_HEIGHT - half and d.BALL_MACHINE_MUZZLE_HEIGHT + half <= body_z[1])
        self.assertGreater(d.BALL_MACHINE_WHEEL_DIAMETER, d.BALL_MACHINE_CLEARANCE)
        self.assertLess(d.BALL_MACHINE_HOPPER[0], d.BALL_MACHINE_BODY[0])
        self.assertLess(d.BALL_MACHINE_HOPPER[1], d.BALL_MACHINE_BODY[1])


class SmallPropsTest(unittest.TestCase):
    def test_can_holds_three_balls(self):
        inside = d.BALL_CAN_HEIGHT - d.BALL_CAN_LID_HEIGHT
        self.assertGreaterEqual(inside, 3 * 2 * d.BALL_RADIUS)
        self.assertGreater(d.BALL_CAN_DIAMETER, 2 * d.BALL_RADIUS)
        self.assertEqual(d.BALL_CAN_SIDES % 4, 0)

    def test_target_is_above_the_lines_and_inner_square_is_centered(self):
        self.assertGreater(d.SERVE_TARGET_THICKNESS, 0.001)   # ラインは 1 mm 浮かせてある（build_court.py）
        _, (verts, _) = cp.serve_target()
        lo, hi = bounds(verts)
        for axis in range(2):
            self.assertAlmostEqual(hi[axis] - lo[axis], d.SERVE_TARGET_INNER, delta=TOL)
            self.assertAlmostEqual(hi[axis] + lo[axis], 0.0, delta=TOL)

    def test_scoreboard_screen_is_on_the_front_inside_the_frame(self):
        (panel, _), _, (screen, _) = cp.scoreboard()
        (plo, phi), (slo, shi) = bounds(panel), bounds(screen)
        self.assertAlmostEqual(slo[0], phi[0], delta=TOL)      # 盤の正面（+X）に付く
        self.assertGreater(shi[0], phi[0])
        for axis in (1, 2):
            self.assertAlmostEqual(slo[axis] - plo[axis], d.SCOREBOARD_SCREEN_MARGIN, delta=TOL)
            self.assertAlmostEqual(phi[axis] - shi[axis], d.SCOREBOARD_SCREEN_MARGIN, delta=TOL)


if __name__ == "__main__":
    unittest.main()
