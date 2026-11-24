"""import_rules.py の単体テスト（unreal なしで動く）。"""
import sys
import unittest
from pathlib import Path, PurePath

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tennis_toolset.import_rules import (  # noqa: E402
    ART_ROOT, CHARACTERS_ROOT, PLAYER_ANIMATION, PLAYER_SKELETAL, WEAR_SKELETAL, ImportRuleError, is_from_export, plan_import,
)

EXPORT = str(PurePath("C:/proj/Export"))
EXPORTED_BY_BLENDER = [  # Tools/Blender/export_all.py が書き出すファイル
    "SM_Court_Surface.fbx", "SM_Court_Inner.fbx", "SM_Court_Lines.fbx",
    "SM_Net.fbx", "SM_NetBand.fbx", "SM_NetPost.fbx", "SM_SinglesStick.fbx",
    "SM_Ball.fbx", "SM_Ball_LOD1.fbx", "SM_Ball_Large.fbx", "SM_AxisTest.fbx",
    "SM_Racket.fbx", "SM_Practice_Fence.fbx", "SM_Practice_Bench.fbx", "SM_Practice_LightPole.fbx",
    "SM_Prop_UmpireChair.fbx", "SM_Prop_BallMachine.fbx", "SM_Prop_BallCan.fbx", "SM_Prop_Cooler.fbx",
    "SM_Prop_ServeTarget.fbx", "SM_Prop_Scoreboard.fbx",
    "SM_Stadium_A_Stands.fbx", "SM_Stadium_A_Stands_LOD1.fbx", "SM_Stadium_A_Barrier.fbx", "SM_Stadium_A_Floor.fbx",
]


class PlanImportTest(unittest.TestCase):
    def test_destination_is_chosen_by_prefix(self):
        cases = {
            "SM_Court_Lines.fbx": f"{ART_ROOT}/Court",
            "SM_NetPost.fbx": f"{ART_ROOT}/Net",
            "SM_SinglesStick.fbx": f"{ART_ROOT}/Net",
            "SM_Ball_LOD1.fbx": f"{ART_ROOT}/Ball",
            "SM_AxisTest.fbx": f"{ART_ROOT}/Dev",
            "SM_Racket.fbx": f"{ART_ROOT}/Racket",
            "SM_Practice_LightPole.fbx": f"{ART_ROOT}/Stadium",
            "SM_Prop_BallMachine.fbx": f"{ART_ROOT}/Props",
        }
        for name, destination in cases.items():
            with self.subTest(name=name):
                plan = plan_import(name, EXPORT)
                self.assertEqual(plan.destination_path, destination)
                self.assertEqual(plan.asset_name, name[:-4])
                self.assertEqual(plan.source_file, str(PurePath(EXPORT) / name))

    def test_court_net_ball_do_not_use_nanite(self):
        # ART-01 §1 の予算表：コート・ネット・ボール・ラケット・練習コートの小物は Nanite を使わない（モバイルの制約 NFR-MOB-002 も）
        for name in EXPORTED_BY_BLENDER:
            with self.subTest(name=name):
                self.assertFalse(plan_import(name, EXPORT).nanite)

    def test_racket_and_practice_props_have_no_collision(self):
        # ART-03 §5：ラケットと練習コートの小物はコリジョンなし。ほか（選手が立つコートなど）は取り込みのまま残す
        without = {"SM_Racket.fbx", "SM_Practice_Fence.fbx", "SM_Practice_Bench.fbx", "SM_Practice_LightPole.fbx"}
        for name in EXPORTED_BY_BLENDER:
            with self.subTest(name=name):
                expected = name not in without and not name.startswith(("SM_Prop_", "SM_Stadium_"))  # 小物とスタジアムもコリジョンなし
                self.assertEqual(plan_import(name, EXPORT).collision, expected)

    def test_every_blender_export_has_a_destination(self):
        for name in EXPORTED_BY_BLENDER:
            with self.subTest(name=name):
                plan = plan_import(name, EXPORT)
                self.assertTrue(plan.destination_path.startswith(ART_ROOT + "/"))
                self.assertIsNone(plan.skeletal)  # スタティックメッシュ

    def test_stadium_lods_are_merged_into_the_base_mesh(self):
        # ART-03 §5：スタジアムの _LOD<n> は、元のメッシュの LOD n に入れる。ボールの LOD1 は別のアセットのまま
        plan = plan_import("SM_Stadium_A_Stands_LOD1.fbx", EXPORT)
        self.assertEqual((plan.destination_path, plan.lod_of, plan.lod_index), (f"{ART_ROOT}/Stadium", "SM_Stadium_A_Stands", 1))
        for name in ("SM_Stadium_A_Stands.fbx", "SM_Stadium_A_Barrier.fbx", "SM_Ball_LOD1.fbx", "SM_Stadium_A_LOD0.fbx", "SM_Stadium_A_LOD8.fbx"):
            with self.subTest(name=name):
                self.assertIsNone(plan_import(name, EXPORT).lod_of)

    def test_center_a_prop_lods_are_merged(self):
        for lod in (1, 2):
            plan = plan_import(f"SM_Prop_A_BroadcastCamera_LOD{lod}.fbx", EXPORT)
            self.assertEqual((plan.destination_path, plan.lod_of, plan.lod_index),
                             (f"{ART_ROOT}/Props", "SM_Prop_A_BroadcastCamera", lod))
        self.assertIsNone(plan_import("SM_Prop_BallCan_LOD1.fbx", EXPORT).lod_of)

    def test_players_go_to_the_existing_skeleton(self):
        # ART-03 §5：仮の選手（ART-01 A-CHR-004・005）は既存の骨格と物理アセットに入れ、LOD を 4 つ作る
        for name in ("SKM_Player_Male01.fbx", "SKM_Player_Female01.fbx"):
            with self.subTest(name=name):
                plan = plan_import(name, EXPORT)
                self.assertEqual(plan.destination_path, f"{CHARACTERS_ROOT}/Players/Interim")
                self.assertEqual(plan.asset_name, name[:-4])
                self.assertFalse(plan.nanite)
                self.assertEqual(plan.skeletal, PLAYER_SKELETAL)
        self.assertEqual(PLAYER_SKELETAL.skeleton, f"{CHARACTERS_ROOT}/Base/SK_TennisPlayer_Base")
        self.assertEqual(PLAYER_SKELETAL.physics_asset, f"{CHARACTERS_ROOT}/Players/Mannequin/PA_Mannequin")
        self.assertEqual(PLAYER_SKELETAL.lod_count, 4)  # ART-01 §1 の予算表

    def test_wear_goes_to_the_metahuman_skeleton(self):
        # ART-03 §6 の手順 5：MetaHuman の選手の服は、組み立てた体の共通の骨格に入れ、物理アセットは付けず、LOD は体と同じ 3 つ
        for name in ("SKM_Wear_M01.fbx", "SKM_Wear_F01.fbx"):
            with self.subTest(name=name):
                plan = plan_import(name, EXPORT)
                self.assertEqual(plan.destination_path, f"{CHARACTERS_ROOT}/Players/MetaHuman/Wear")
                self.assertEqual(plan.asset_name, name[:-4])
                self.assertFalse(plan.nanite)
                self.assertIs(plan.skeletal, WEAR_SKELETAL)
        self.assertEqual(WEAR_SKELETAL.skeleton,
                         f"{CHARACTERS_ROOT}/Players/MetaHuman/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel")
        self.assertIsNone(WEAR_SKELETAL.physics_asset)
        self.assertEqual(WEAR_SKELETAL.lod_count, 3)

    def test_stroke_animations_go_to_the_existing_skeleton(self):
        # Tools/Blender/build_stroke_anims.py が書き出すアニメーション。骨格は選手のメッシュと同じ
        for name in ("AS_FH_Mid_Drive_Square.fbx", "AS_BH1_Mid_Drive_Square.fbx"):
            with self.subTest(name=name):
                plan = plan_import(name, EXPORT)
                self.assertEqual(plan.destination_path, f"{CHARACTERS_ROOT}/Animations/Strokes")
                self.assertIs(plan.animation, PLAYER_ANIMATION)
                self.assertEqual(plan.animation.skeleton, PLAYER_SKELETAL.skeleton)
                self.assertIsNone(plan.skeletal)
                self.assertIsNone(plan.lod_of)

    def test_other_animations_are_rejected(self):
        # 命名にない種類（BH）・ロコモーション・モンタージュ（UE で作る）は取り込まない
        for name in ("AS_BH_Mid_Drive_Square.fbx", "AS_Idle.fbx", "AM_FH_Mid_Drive_Square.fbx"):
            with self.subTest(name=name), self.assertRaises(ImportRuleError):
                plan_import(name, EXPORT)

    def test_paths_and_traversal_are_rejected(self):
        for name in ("../secret.fbx", "C:/x/SM_Ball.fbx", "sub/SM_Ball.fbx", "..\\SM_Ball.fbx"):
            with self.subTest(name=name), self.assertRaises(ImportRuleError):
                plan_import(name, EXPORT)

    def test_other_extensions_are_rejected(self):
        for name in ("SM_Ball.obj", "SM_Ball.fbx.exe", "SM_Ball", ""):
            with self.subTest(name=name), self.assertRaises(ImportRuleError):
                plan_import(name, EXPORT)

    def test_unknown_prefix_is_rejected(self):
        for name in ("SK_Player.fbx", "SKM_Manny_Simple.fbx"):  # 元の体（Export/Base/）は取り込まない
            with self.subTest(name=name), self.assertRaises(ImportRuleError):
                plan_import(name, EXPORT)


class IsFromExportTest(unittest.TestCase):
    def test_files_directly_in_export_are_preview_sources(self):
        for files in (["C:/proj/Export/SM_Court_Lines.fbx"], ["C:\\proj\\Export\\SM_Net.fbx"],
                      ["D:/other/a.fbx", "C:/proj/Export/SM_Ball.fbx"]):
            with self.subTest(files=files):
                self.assertTrue(is_from_export(files, EXPORT))

    def test_other_locations_are_not_touched(self):
        for files in ([], [""], ["C:/proj/Export2/SM_Ball.fbx"], ["C:/proj/Export/sub/SM_Ball.fbx"],
                      ["C:/proj/Export/../Art/SM_Ball.fbx"], ["C:/proj/SM_Ball.fbx"]):
            with self.subTest(files=files):
                self.assertFalse(is_from_export(files, EXPORT))


if __name__ == "__main__":
    unittest.main()
