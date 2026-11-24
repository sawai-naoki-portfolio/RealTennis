"""court_level_layout.py の単体テスト：取り込みの規則と食い違わないこと（DRY）を確かめる。"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tennis_toolset import court_level_layout as layout  # noqa: E402
from tennis_toolset.import_rules import ART_ROOT, plan_import  # noqa: E402


class CourtLevelLayoutTest(unittest.TestCase):
    def test_every_mesh_has_an_import_destination(self):
        for placement in layout.MESHES:
            with self.subTest(asset=placement.asset_name):
                plan = plan_import(placement.asset_name + ".fbx", "Export")
                self.assertTrue(plan.destination_path.startswith(ART_ROOT + "/"))

    def test_asset_names_are_unique(self):
        names = [p.asset_name for p in layout.MESHES]
        self.assertEqual(len(names), len(set(names)))

    def test_court_and_net_are_at_the_origin(self):
        for placement in layout.MESHES:
            if placement.asset_name.startswith(("SM_Court_", "SM_Net", "SM_SinglesStick")):
                with self.subTest(asset=placement.asset_name):
                    self.assertEqual(placement.location_cm, (0.0, 0.0, 0.0))

    def test_ball_mesh_has_an_import_destination(self):
        plan = plan_import(layout.BALL.mesh_asset + ".fbx", "Export")
        self.assertTrue(plan.destination_path.startswith(ART_ROOT + "/"))

    def test_practice_court_props_are_at_the_origin(self):
        names = [p.asset_name for p in layout.MESHES]
        for name in ("SM_Practice_Fence", "SM_Practice_Bench", "SM_Practice_LightPole"):
            with self.subTest(asset=name):
                self.assertIn(name, names)
                self.assertEqual(layout.MESHES[names.index(name)].location_cm, (0.0, 0.0, 0.0))

    def test_ball_waits_behind_the_opponent_baseline_above_the_ground(self):
        half_length_cm = 2377.0 / 2.0  # Rule 1（TennisConstants.h の HalfLength）
        self.assertGreater(layout.BALL.location_cm[0], half_length_cm - 1.0)
        self.assertGreater(layout.BALL.location_cm[2], 0.0)

    def test_ball_is_not_also_placed_as_a_static_mesh(self):
        self.assertNotIn(layout.BALL.mesh_asset, [p.asset_name for p in layout.MESHES])

    def test_shot_tester_hits_from_inside_the_opponent_baseline_towards_the_player(self):
        half_length_m = 23.77 / 2.0  # Rule 1（TennisConstants.h の HalfLength）
        x, _, z = layout.SHOT_TESTER.hit_point_m
        self.assertTrue(0.0 < x < half_length_m)                # 相手の側（ボールが待つ側）
        self.assertLess(layout.PLAYER_START_LOCATION_CM[0], 0.0)  # 選手はネットの反対の側
        self.assertGreater(z, 0.0)
        self.assertTrue(layout.SHOT_TESTER.shot_type_folder.startswith("/Game/Tennis/Data/"))

    def test_player_starts_behind_the_baseline_inside_their_area(self):
        half_length_cm = 2377.0 / 2.0  # Rule 1
        behind_cm = 600.0              # DD-03 §2.4（ベースラインの後ろ 6 m まで）
        x, y, z = layout.PLAYER_START_LOCATION_CM
        self.assertTrue(-(half_length_cm + behind_cm) < x < -half_length_cm)
        self.assertGreater(z, 90.0)  # カプセルの半分の高さより上
        self.assertEqual(layout.PLAYER_START_ROTATION[1], 0.0)  # ネット（+X）の方を向く

    def test_level_is_under_tennis_maps(self):
        self.assertTrue(layout.LEVEL_PATH.startswith("/Game/Tennis/Maps/L_"))

    def test_has_light_for_viewing(self):
        self.assertIn("DirectionalLight", [name for name, _ in layout.LIGHTS])


if __name__ == "__main__":
    unittest.main()
