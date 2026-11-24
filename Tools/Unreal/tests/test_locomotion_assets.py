"""locomotion_assets.py の単体テスト（unreal なしで動く）。"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tennis_toolset import locomotion_assets as la  # noqa: E402
from tennis_toolset import stroke_assets as sa  # noqa: E402


class LocomotionAssetsTest(unittest.TestCase):
    def test_assets_live_with_the_locomotion_animations(self):
        # ARCH-02 §4：Characters/Animations/Locomotion に PSD_Locomotion（PoseSearch のデータベース）
        self.assertEqual(la.LOCOMOTION_FOLDER, "/Game/Tennis/Characters/Animations/Locomotion")
        self.assertEqual(la.SCHEMA_PATH, la.LOCOMOTION_FOLDER + "/PSS_Locomotion")
        self.assertEqual(la.DATABASE_PATH, la.LOCOMOTION_FOLDER + "/PSD_Locomotion")
        self.assertEqual(la.SKELETON_PATH, "/Game/Tennis/Characters/Base/SK_TennisPlayer_Base")

    def test_placeholders_are_the_idle_and_eight_walk_and_jog_directions(self):
        # DD-03 §6.5、ART-01 A-CHR-003：待機と、歩き・ジョグの 8 方向ずつの 17 本
        self.assertEqual(len(la.DIRECTIONS), 8)
        self.assertEqual(len(set(la.DIRECTIONS)), 8)
        self.assertEqual(len(la.PLACEHOLDER_ANIMATIONS), 17)
        self.assertEqual(len(set(la.PLACEHOLDER_ANIMATIONS)), 17)
        self.assertEqual(la.PLACEHOLDER_ANIMATIONS[0], "Mannequin/MM_Idle")
        for gait in ("Walk", "Jog"):
            names = [n for n in la.PLACEHOLDER_ANIMATIONS if f"/{gait}/" in n]
            self.assertEqual(sorted(n.split(f"MF_Unarmed_{gait}_")[1] for n in names), sorted(la.DIRECTIONS))

    def test_animation_paths_are_under_the_folder_in_order(self):
        paths = la.animation_paths()
        self.assertEqual(len(paths), 17)
        self.assertTrue(all(p.startswith(la.LOCOMOTION_FOLDER + "/Mannequin/") for p in paths))
        self.assertEqual(paths[1], la.LOCOMOTION_FOLDER + "/Mannequin/Walk/MF_Unarmed_Walk_Fwd")

    def test_owner_tag_is_shared_with_the_other_tools(self):
        self.assertIs(la.OWNER_TAG, sa.OWNER_TAG)  # 1 か所で決める
        self.assertEqual(la.OWNER_VALUE, "build_locomotion_database")
        self.assertNotEqual(la.OWNER_VALUE, sa.OWNER_VALUE)  # 持ち主のツールが分かる


if __name__ == "__main__":
    unittest.main()
