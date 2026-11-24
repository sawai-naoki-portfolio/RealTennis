"""metahuman_players.py の単体テスト（unreal なしで動く）。"""
import sys
import unittest
from dataclasses import replace
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tennis_toolset import metahuman_players as mp  # noqa: E402


class MetaHumanPlayersTest(unittest.TestCase):
    def test_players_are_the_two_chosen_presets(self):
        # 利用者の判断（2026-11-14）：男性 M01 は Kelvin（髪はそのまま）、女性 F01 は Celeste（髪はポニーテール）
        self.assertEqual([(p.key, p.preset, p.hair) for p in mp.PLAYERS],
                         [("M01", "Kelvin", None), ("F01", "Celeste", "WI_Hair_S_LowPonytail")])
        for player in mp.PLAYERS:
            mp.validate(player)

    def test_heights_follow_the_asset_list(self):
        # ART-01 A-CHR-010・011：男性 185 cm、女性 175 cm、どちらも筋肉質（筋肉の値は正）
        body = {p.key: dict(p.body) for p in mp.PLAYERS}
        self.assertEqual(body["M01"]["Height"], 185.0)
        self.assertEqual(body["F01"]["Height"], 175.0)
        self.assertTrue(all(b["Muscularity"] > 0 for b in body.values()))

    def test_paths_share_one_common_folder(self):
        m01 = mp.find_player("M01")
        self.assertEqual(m01.character_path, "/Game/Tennis/Characters/Players/MetaHuman/MHC_Player_M01")
        self.assertEqual(m01.appearance_path, "/Game/Tennis/Characters/Players/MetaHuman/DA_Appearance_Player_M01")
        self.assertEqual(m01.preset_path, "/MetaHumanCharacter/Optional/Presets/Kelvin")
        self.assertIsNone(m01.hair_path)
        self.assertEqual(mp.find_player("F01").hair_path, "/MetaHumanCharacter/Optional/Grooms/Bindings/Hair/WI_Hair_S_LowPonytail")
        self.assertEqual(mp.COMMON_FOLDER, "/Game/Tennis/Characters/Players/MetaHuman/Common")
        # 見た目のデータは、焼くツール（bake_player_appearances）が探す選手のフォルダの下
        self.assertTrue(m01.appearance_path.startswith("/Game/Tennis/Characters/Players/"))

    def test_body_and_wear_paths(self):
        # 体は組み立てのフォルダ、服（A-CHR-040〜043）は共通の Wear/ に選手ごとに 1 つ（ART-03 §6 の手順 5）
        f01 = mp.find_player("F01")
        self.assertEqual(f01.body_mesh_path, "/Game/Tennis/Characters/Players/MetaHuman/MHC_Player_F01/Body/SKM_MHC_Player_F01_BodyMesh")
        self.assertEqual(f01.face_mesh_path, "/Game/Tennis/Characters/Players/MetaHuman/MHC_Player_F01/Face/SKM_MHC_Player_F01_FaceMesh")
        self.assertEqual(f01.wear_path, "/Game/Tennis/Characters/Players/MetaHuman/Wear/SKM_Wear_F01")
        self.assertEqual(mp.SKELETON, f"{mp.COMMON_FOLDER}/Female/Medium/NormalWeight/Body/metahuman_base_skel")

    def test_find_player_rejects_unknown_or_malformed_keys(self):
        for key in ("M02", "m01", "M1", "../M01", "M01/..", "", None, 1):
            with self.assertRaises(mp.MetaHumanPlayerError):
                mp.find_player(key)

    def test_built_paths_must_be_in_their_folders(self):
        m01 = mp.find_player("M01")
        body = f"{m01.build_folder}/Body/SKM_MHC_Player_M01_BodyMesh.SKM_MHC_Player_M01_BodyMesh"
        skeleton = f"{mp.COMMON_FOLDER}/Female/Medium/NormalWeight/Body/metahuman_base_skel.metahuman_base_skel"
        mp.check_built_paths(m01, body, skeleton)
        for bad_body, bad_skeleton in ((body.replace("M01", "F01"), skeleton),
                                       (body, "/MetaHumanCharacter/Body/metahuman_base_skel.metahuman_base_skel"),
                                       (body, mp.COMMON_FOLDER + "Other/metahuman_base_skel"),
                                       (m01.build_folder + "Extra/Body/X", skeleton)):
            with self.assertRaises(mp.MetaHumanPlayerError):
                mp.check_built_paths(m01, bad_body, bad_skeleton)

    def test_validate_rejects_bad_values(self):
        good = mp.PLAYERS[0]
        bad = (
            replace(good, key="X01"),
            replace(good, preset="../Kelvin"),
            replace(good, hair="WI Hair"),
            replace(good, body=(("Height", 300.0),)),
            replace(good, body=(("Height", 180.0), ("Height", 181.0))),
            replace(good, body=(("Shoe Size", 10.0),)),
            replace(good, body=(("Muscularity", "1"),)),
        )
        for player in bad:
            with self.assertRaises(mp.MetaHumanPlayerError, msg=str(player)):
                mp.validate(player)


if __name__ == "__main__":
    unittest.main()
