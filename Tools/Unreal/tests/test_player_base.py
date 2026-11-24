"""player_base.py の単体テスト（unreal なしで動く）。"""
import sys
import unittest
from pathlib import Path, PurePath

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tennis_toolset.import_rules import ImportRuleError, plan_import  # noqa: E402
from tennis_toolset.player_base import BASE_MESHES, BASE_SUBDIR, base_fbx_path  # noqa: E402

EXPORT = str(PurePath("C:/proj/Export"))


class BaseFbxPathTest(unittest.TestCase):
    def test_bodies_go_to_export_base(self):
        # Manny・Quinn（ART-01 A-CHR-002）と、MetaHuman の選手の組み立てた体（A-CHR-010・011。服を作る元：ART-03 §6 の手順 5）
        self.assertEqual(set(BASE_MESHES), {"SKM_Manny_Simple", "SKM_Quinn_Simple",
                                            "SKM_MHC_Player_M01_BodyMesh", "SKM_MHC_Player_F01_BodyMesh"})
        for name in BASE_MESHES:
            with self.subTest(name=name):
                self.assertEqual(base_fbx_path(EXPORT, name), str(PurePath(EXPORT) / BASE_SUBDIR / f"{name}.fbx"))

    def test_base_meshes_live_in_their_folders(self):
        for name in ("SKM_Manny_Simple", "SKM_Quinn_Simple"):
            with self.subTest(name=name):
                self.assertEqual(BASE_MESHES[name], f"/Game/Tennis/Characters/Players/Mannequin/{name}")
        for key in ("M01", "F01"):
            with self.subTest(key=key):
                self.assertEqual(BASE_MESHES[f"SKM_MHC_Player_{key}_BodyMesh"],
                                 f"/Game/Tennis/Characters/Players/MetaHuman/MHC_Player_{key}/Body/SKM_MHC_Player_{key}_BodyMesh")

    def test_unknown_mesh_is_rejected(self):
        for name in ("SKM_Player_Male01", "../SKM_Manny_Simple", ""):
            with self.subTest(name=name), self.assertRaises(KeyError):
                base_fbx_path(EXPORT, name)

    def test_base_files_are_not_imported_back(self):
        # Export/Base/ はファイル名だけを受け付ける取り込みの対象にならない（元の体を上書きしない）
        for name in BASE_MESHES:
            with self.subTest(name=name), self.assertRaises(ImportRuleError):
                plan_import(f"{BASE_SUBDIR}/{name}.fbx", EXPORT)


if __name__ == "__main__":
    unittest.main()
