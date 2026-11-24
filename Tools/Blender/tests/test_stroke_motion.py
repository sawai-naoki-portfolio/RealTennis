"""stroke_motion.py：キーの補間、ラケットの向き、2 本の骨の IK、仮のフォア・バックの動きの約束。"""
import math
import re
import sys
import unittest
from pathlib import Path

sys.path[:0] = [str(Path(__file__).resolve().parents[1])]

from tennis_blender import stroke_motion as sm  # noqa: E402


class InterpolationTest(unittest.TestCase):
    def test_sample_hits_the_keys(self):
        for motion in sm.MOTIONS:
            for key in motion.keys:
                got = sm.sample(motion, key.frame)
                self.assertAlmostEqual(got.chest_yaw, key.chest_yaw)
                self.assertAlmostEqual(got.pelvis_drop, key.pelvis_drop)
                for a in range(3):
                    self.assertAlmostEqual(got.sweet_spot[a], key.sweet_spot[a])
                    self.assertAlmostEqual(got.off_wrist[a], key.off_wrist[a])

    def test_directions_are_unit_and_motion_is_continuous(self):
        for motion in sm.MOTIONS:
            previous = sm.sample(motion, 0)
            for frame in range(1, motion.last_frame + 1):
                cur = sm.sample(motion, frame)
                self.assertAlmostEqual(sm.length(cur.racket_dir), 1.0)
                self.assertAlmostEqual(sm.length(cur.face_normal), 1.0)
                # 1 フレーム（1/30 s）で、スイートスポットは 1.2 m より動かない（36 m/s。飛びがない）
                self.assertLess(sm.length(sm.sub(cur.sweet_spot, previous.sweet_spot)), 1.2, (motion.name, frame))
                previous = cur

    def test_outside_the_range_holds_the_end_keys(self):
        motion = sm.FOREHAND
        self.assertEqual(sm.sample(motion, -5).sweet_spot, motion.keys[0].sweet_spot)
        self.assertEqual(sm.sample(motion, motion.last_frame + 9).sweet_spot, motion.keys[-1].sweet_spot)


class RacketFrameTest(unittest.TestCase):
    def test_frame_is_orthonormal_and_right_handed(self):
        x, y, z = sm.racket_frame((1.0, 0.2, 0.0), (0.3, 0.1, 1.0))
        for v in (x, y, z):
            self.assertAlmostEqual(sm.length(v), 1.0)
        self.assertAlmostEqual(sm.dot(x, y), 0.0)
        self.assertAlmostEqual(sm.dot(x, z), 0.0)
        self.assertAlmostEqual(sm.dot(sm.cross(x, y), z), 1.0)

    def test_grip_is_behind_the_sweet_spot(self):
        key = sm.sample(sm.FOREHAND, sm.FOREHAND.impact_frame)
        grip = sm.grip_position(key)
        self.assertAlmostEqual(sm.length(sm.sub(key.sweet_spot, grip)), sm.GRIP_TO_SWEET_SPOT)
        self.assertAlmostEqual(sm.dot(sm.normalized(sm.sub(key.sweet_spot, grip)), key.racket_dir), 1.0)

    def test_socket_axes_match_the_asset_list(self):
        # ART-01 §3.1：ラケットの先端は手の +Z から指の側（手の −X）へ 25°。面の法線は手のひら（UE の +Y＝Blender の骨の −Y）
        self.assertAlmostEqual(sm.length(sm.SOCKET_RACKET_AXIS_IN_HAND), 1.0)
        self.assertAlmostEqual(math.degrees(math.atan2(-sm.SOCKET_RACKET_AXIS_IN_HAND[0], sm.SOCKET_RACKET_AXIS_IN_HAND[2])), 25.0)
        self.assertEqual(sm.SOCKET_FACE_NORMAL_IN_HAND, (0.0, -1.0, 0.0))
        self.assertEqual(sm.SOCKET_LOCATION_IN_HAND, (-0.065, -0.030, 0.0))


class TwoBoneIkTest(unittest.TestCase):
    def test_reachable_target_is_hit_with_exact_lengths(self):
        root, target = (0.0, 0.0, 1.4), (-0.3, -0.2, 1.1)
        joint, tip = sm.two_bone_ik(root, target, 0.28, 0.27, (0.0, 0.5, -1.0))
        self.assertAlmostEqual(sm.length(sm.sub(joint, root)), 0.28)
        self.assertAlmostEqual(sm.length(sm.sub(tip, joint)), 0.27)
        for a in range(3):
            self.assertAlmostEqual(tip[a], target[a])
        # 関節は pole の側へ曲がる
        mid = tuple((root[a] + target[a]) / 2 for a in range(3))
        self.assertGreater(sm.dot(sm.sub(joint, mid), (0.0, 0.5, -1.0)), 0.0)

    def test_far_target_is_clamped_to_an_almost_straight_limb(self):
        root = (0.0, 0.0, 1.4)
        joint, tip = sm.two_bone_ik(root, (-2.0, 0.0, 1.4), 0.28, 0.27, (0.0, 0.5, -1.0))
        self.assertAlmostEqual(sm.length(sm.sub(tip, root)), 0.55 * 0.999)
        self.assertAlmostEqual(sm.length(sm.sub(joint, root)), 0.28)
        self.assertAlmostEqual(sm.length(sm.sub(tip, joint)), 0.27, places=6)

    def test_degenerate_inputs_are_rejected(self):
        with self.assertRaises(ValueError):
            sm.two_bone_ik((0.0, 0.0, 0.0), (0.0, 0.0, 0.0), 0.3, 0.3, (0.0, 1.0, 0.0))
        with self.assertRaises(ValueError):
            sm.two_bone_ik((0.0, 0.0, 0.0), (0.4, 0.0, 0.0), 0.3, 0.3, (1.0, 0.0, 0.0))


class MotionsTest(unittest.TestCase):
    def test_strokes_start_and_end_in_the_same_ready_pose(self):
        for motion in sm.MOTIONS:
            first, last = motion.keys[0], motion.keys[-1]
            self.assertEqual((first.sweet_spot, first.racket_dir, first.chest_yaw, first.pelvis_yaw, first.off_wrist),
                             (last.sweet_spot, last.racket_dir, last.chest_yaw, last.pelvis_yaw, last.off_wrist), motion.name)
            self.assertEqual(first.frame, 0)

    def test_impact_is_in_front_and_the_face_points_to_the_net(self):
        # インパクト：フォアは右（−X）、バックは左（+X）。体より前（−Y）、腰〜胸の高さ。面は前（−Y）を向く
        for motion, side in ((sm.FOREHAND, -1.0), (sm.BACKHAND, 1.0), (sm.FOREHAND_DIP, -1.0), (sm.BACKHAND_DIP, 1.0)):
            key = sm.sample(motion, motion.impact_frame)
            self.assertGreater(side * key.sweet_spot[0], 0.5, motion.name)
            self.assertLess(key.sweet_spot[1], -0.2, motion.name)
            self.assertTrue(0.6 <= key.sweet_spot[2] <= 1.3, motion.name)  # 低く打つ変種（Dip）を含む
            _, _, normal = sm.racket_frame(key.racket_dir, key.face_normal)
            self.assertLess(normal[1], -0.8, motion.name)

    def test_notify_times_follow_the_frames(self):
        for motion in sm.MOTIONS:
            times = sm.notify_times(motion)
            self.assertEqual(times["name"], motion.name)
            self.assertAlmostEqual(times["impact_time"], motion.impact_frame / 30)
            self.assertTrue(0 < motion.impact_frame < motion.recover_frame < motion.last_frame)
            self.assertEqual(times["frames"], motion.last_frame)

    def test_names_follow_the_animation_naming(self):
        # ART-02 §1：<種類>_<高さ>_<ファミリー>_<スタンス・到達>。取り込んだ後のモンタージュは AM_ に変えた名前になる
        naming = re.compile(r"AS_(FH|BH1|BH2|VO|HV|SM|SV|RT)_(VLow|Low|Mid|High|Over)_(Drive|Slice|Lob|Drop|Block)"
                            r"_(Open|Semi|Square|Closed|Stretch|Dive|Slide)(_Dip)?")
        for motion in sm.MOTIONS:
            self.assertIsNotNone(naming.fullmatch(motion.name), motion.name)


class DipTest(unittest.TestCase):
    def test_dip_lowers_the_swing_and_keeps_the_ready_pose(self):
        # 低く打つ変種（DD-03 §5.3）：構えは同じ、ほかのキーはスイートスポットを drop だけ下げ、腰を落とす。時刻は同じ
        for base, dip in ((sm.FOREHAND, sm.FOREHAND_DIP), (sm.BACKHAND, sm.BACKHAND_DIP)):
            self.assertEqual(dip.name, base.name + "_Dip")
            self.assertEqual((dip.impact_frame, dip.recover_frame), (base.impact_frame, base.recover_frame))
            self.assertEqual((dip.keys[0], dip.keys[-1]), (base.keys[0], base.keys[-1]))
            for b, d in zip(base.keys[1:-1], dip.keys[1:-1]):
                self.assertEqual(d.frame, b.frame)
                self.assertLess(d.sweet_spot[2], b.sweet_spot[2] - 0.15)
                self.assertEqual(d.sweet_spot[:2], b.sweet_spot[:2])
                self.assertGreater(d.pelvis_drop, b.pelvis_drop)
                self.assertEqual((d.racket_dir, d.face_normal), (b.racket_dir, b.face_normal))

    def test_dipped_rejects_nothing_and_is_pure(self):
        before = sm.FOREHAND
        sm.dipped(sm.FOREHAND, "AS_FH_Mid_Drive_Square_Dip", drop=0.2)
        self.assertIs(sm.FOREHAND, before)  # 元の動きは変えない
        self.assertEqual(sm.dipped(sm.FOREHAND, "X", drop=0.0).keys, sm.FOREHAND.keys)


if __name__ == "__main__":
    unittest.main()
