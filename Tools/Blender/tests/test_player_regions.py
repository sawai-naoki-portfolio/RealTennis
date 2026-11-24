"""player_regions.py・player_specs.py：体の頂点の分け方（服・消す部分・肌）、襟ぐり、手、2 人の仕様。"""
import sys
import unittest
from pathlib import Path

sys.path[:0] = [str(Path(__file__).resolve().parents[1]), str(Path(__file__).resolve().parent)]

from tennis_blender import materials as m  # noqa: E402
from tennis_blender import player_regions as pr  # noqa: E402
from tennis_blender import player_specs as ps  # noqa: E402

# 骨の付け根（Manny の値に近い、m。選手は −Y を向く）
BONES = {"pelvis": (0.0, 0.0, 0.96), "spine_05": (0.0, 0.0, 1.41), "neck_01": (0.0, 0.02, 1.53),
         "upperarm_l": (0.19, 0.03, 1.44), "lowerarm_l": (0.35, 0.02, 1.21), "hand_l": (0.48, -0.16, 1.05),
         "thigh_l": (0.10, -0.03, 0.94), "calf_l": (0.12, -0.03, 0.50)}
LM = pr.Landmarks(bones=BONES, neck_axis=(0.0, 0.01), collar_z=1.41)


class ClassifyTest(unittest.TestCase):
    male, female = ps.MALE01.outfit, ps.FEMALE01.outfit

    def tags(self, p, bone, outfit=None):
        return pr.classify(p, bone, LM, outfit or self.male)

    def test_old_head_and_the_neck_inside_the_collar_are_removed(self):
        self.assertEqual(self.tags((0.0, 0.0, 1.70), "head"), {pr.REMOVE})
        self.assertEqual(self.tags((0.0, -0.05, 1.50), "spine_05"), {pr.REMOVE})
        self.assertEqual(self.tags((0.15, 0.0, 1.50), "spine_05"), {pr.TOP})  # 肩はシャツ

    def test_front_of_the_collar_is_wider(self):
        back, front = (0.0, 0.10, 1.5), (0.0, -0.08, 1.5)
        self.assertGreater(pr.collar_radius(front, LM, 0.05), pr.collar_radius(back, LM, 0.05))
        self.assertAlmostEqual(pr.collar_radius(back, LM, 0.05), pr.COLLAR_R)

    def test_waist_is_covered_by_both_shirt_and_shorts(self):
        self.assertEqual(self.tags((0.1, 0.0, 0.98), "pelvis"), {pr.TOP, pr.BOTTOM})
        self.assertEqual(self.tags((0.1, 0.0, 0.90), "pelvis"), {pr.BOTTOM})
        self.assertIn(pr.TOP, self.tags((0.15, 0.0, 0.97), "thigh_l"))  # 腰の横に穴を作らない

    def test_sleeves_and_shorts_end_at_their_length(self):
        arm = lambda t: tuple(a + (b - a) * t for a, b in zip(BONES["upperarm_l"], BONES["lowerarm_l"]))  # noqa: E731
        self.assertIn(pr.TOP, self.tags(arm(0.3), "upperarm_twist_01_l"))
        self.assertEqual(self.tags(arm(0.8), "upperarm_twist_02_l"), {pr.SMOOTH})
        self.assertEqual(self.tags(arm(0.3), "upperarm_l", self.female), {pr.SMOOTH})  # 袖なし
        leg = lambda t: tuple(a + (b - a) * t for a, b in zip(BONES["thigh_l"], BONES["calf_l"]))  # noqa: E731
        self.assertEqual(self.tags(leg(0.4), "thigh_twist_01_l"), {pr.BOTTOM})
        self.assertEqual(self.tags(leg(0.4), "thigh_twist_01_l", self.female), {pr.SMOOTH})

    def test_feet_socks_and_wrists(self):
        self.assertEqual(self.tags((0.15, -0.1, 0.02), "ball_l"), {pr.SHOE})
        self.assertEqual(self.tags((0.15, 0.0, 0.05), "calf_twist_01_l"), {pr.SHOE})
        self.assertEqual(self.tags((0.15, 0.0, 0.15), "calf_twist_01_l"), {pr.SOCK, pr.SMOOTH})
        self.assertEqual(self.tags((0.15, 0.0, 0.15), "calf_twist_01_l", self.female), {pr.SMOOTH})
        wrist = tuple(a + (b - a) * 0.95 for a, b in zip(BONES["lowerarm_l"], BONES["hand_l"]))
        self.assertIn(pr.WRIST, self.tags(wrist, "lowerarm_twist_01_l"))

    def test_hands_are_kept_from_the_source(self):
        self.assertTrue(all(pr.is_hand(b) for b in ("hand_l", "index_02_r", "thumb_01_l", "pinky_03_r")))
        self.assertFalse(any(pr.is_hand(b) for b in ("lowerarm_l", "head", "ball_l")))


class SpecsTest(unittest.TestCase):
    def test_two_players_on_manny_and_quinn(self):
        self.assertEqual([s.name for s in ps.PLAYERS], ["SKM_Player_Male01", "SKM_Player_Female01"])
        self.assertEqual({s.base for s in ps.PLAYERS}, {"SKM_Manny_Simple", "SKM_Quinn_Simple"})
        for s in ps.PLAYERS:
            with self.subTest(player=s.name):
                self.assertEqual(s.max_triangles, 60_000)  # ART-01 §1
                self.assertTrue(all(c in m.PREVIEW_COLORS for c in (s.skin, s.hair_color, s.top, s.accent)))
                self.assertTrue(0.0 < s.outfit.shorts < 1.0 and s.outfit.sock_top > ps.SHOE_TOP)


if __name__ == "__main__":
    unittest.main()
