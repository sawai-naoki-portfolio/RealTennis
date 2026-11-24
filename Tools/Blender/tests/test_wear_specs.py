"""wear_specs.py・player_regions.manny_bone：MetaHuman の選手のテニスウェアの仕様と、補助の骨の分け方。"""
import sys
import unittest
from pathlib import Path

sys.path[:0] = [str(Path(__file__).resolve().parents[1]), str(Path(__file__).resolve().parent)]

from tennis_blender import player_regions as pr  # noqa: E402
from tennis_blender import player_specs as ps  # noqa: E402
from tennis_blender import wear_specs as ws  # noqa: E402

# MetaHuman の体の骨の一部（2026-11-18 に M01 の体で確かめた親子）
PARENTS = {
    "root": None, "pelvis": "root", "spine_01": "pelvis", "spine_05": "spine_01",
    "clavicle_l": "spine_05", "clavicle_pec_l": "spine_05", "spine_04_latissimus_l": "spine_05",
    "upperarm_l": "clavicle_l", "upperarm_correctiveRoot_l": "upperarm_l", "upperarm_out_l": "upperarm_correctiveRoot_l",
    "upperarm_twist_01_l": "upperarm_l", "upperarm_twistCor_01_l": "upperarm_twist_01_l",
    "lowerarm_l": "upperarm_l", "hand_l": "lowerarm_l", "wrist_outer_l": "hand_l", "index_01_l": "hand_l",
    "thigh_l": "pelvis", "calf_l": "thigh_l", "calf_correctiveRoot_l": "calf_l", "calf_knee_l": "calf_correctiveRoot_l",
    "foot_l": "calf_l", "ball_l": "foot_l", "indextoe_01_l": "ball_l", "indextoe_02_l": "indextoe_01_l",
    "littletoe_01_l": "ball_l",
}


class MannyBoneTest(unittest.TestCase):
    def test_helper_bones_map_to_their_manny_ancestor(self):
        expected = {
            "upperarm_twistCor_01_l": "upperarm_twist_01_l", "upperarm_out_l": "upperarm_l",
            "clavicle_pec_l": "spine_05", "spine_04_latissimus_l": "spine_05", "calf_knee_l": "calf_l",
            "wrist_outer_l": "hand_l", "index_01_l": "hand_l",
            # 足の指は ball（靴）。名前が手の指（index・ring など）で始まっても手にしない
            "indextoe_02_l": "ball_l", "littletoe_01_l": "ball_l",
        }
        for bone, manny in expected.items():
            with self.subTest(bone=bone):
                self.assertEqual(pr.manny_bone(bone, PARENTS), manny)

    def test_manny_bones_stay_and_unknown_bones_are_empty(self):
        for bone in ("pelvis", "spine_01", "upperarm_l", "calf_l", "foot_l", "root"):
            with self.subTest(bone=bone):
                self.assertEqual(pr.manny_bone(bone, PARENTS), bone)
        self.assertEqual(pr.manny_bone("FACIAL_C_Jaw", PARENTS), "")
        self.assertEqual(pr.manny_bone("", PARENTS), "")
        self.assertEqual(pr.manny_bone("a", {"a": "b", "b": "a"}), "")  # 親が輪になっていても止まる


class WearSpecsTest(unittest.TestCase):
    def test_wears_are_one_per_metahuman_player(self):
        # 名前は UE の取り込みの規則（SKM_Wear_ → MetaHuman/Wear）、元は export_player_base_fbx が書き出す体
        self.assertEqual([(w.name, w.base) for w in ws.WEARS],
                         [("SKM_Wear_M01", "SKM_MHC_Player_M01_BodyMesh"), ("SKM_Wear_F01", "SKM_MHC_Player_F01_BodyMesh")])

    def test_outfits_follow_the_interim_players_but_no_headband(self):
        # 服装と色は仮の選手と同じ。ヘッドバンドは頭（顔のメッシュ）が要るので後（ART-03 §6）
        self.assertEqual((ws.WEAR_M01.outfit, ws.WEAR_M01.top), (ps.MALE01.outfit, ps.MALE01.top))
        self.assertEqual(ws.WEAR_F01.top, ps.FEMALE01.top)
        self.assertTrue(ws.WEAR_F01.outfit.skirt)
        self.assertFalse(any(w.outfit.headband for w in ws.WEARS))

    def test_shells_never_sink_into_the_body(self):
        # 体の面を消さないので、殻が体の面より内へ入ると肌が出る
        self.assertEqual(ws.SINK, 0.0)
        self.assertGreater(ws.SOCK_OFFSET, 0.0)

    def test_collar_starts_at_the_neck_opening(self):
        # 襟ぐりの後ろは、MetaHuman の体の首の穴（後ろの縁は F01 で 5.7 cm）より小さく、背中を切らない
        self.assertLess(ws.COLLAR_R, 0.057)
        lm = pr.Landmarks(bones={}, neck_axis=(0.0, 0.0), collar_z=1.3, collar_r=ws.COLLAR_R)
        back = (0.0, 0.055, 1.4)  # 選手は −Y を向く。後ろの縁の頂点
        self.assertFalse(pr.near_neck(back, lm, ps.FEMALE01.outfit.neck_dip))
        self.assertTrue(pr.near_neck((0.0, -0.08, 1.4), lm, ps.FEMALE01.outfit.neck_dip))  # 前は neck_dip だけ深い

    def test_sock_overlaps_the_shoe_top(self):
        top = ps.SHOE_TOP
        sock_top = ps.MALE01.outfit.sock_top
        self.assertEqual(ws.foot_regions(top - 0.001, "calf_l", sock_top), (True, True))   # 靴の上端の下は両方（隙間なし）
        self.assertEqual(ws.foot_regions(top - ws.SOCK_UNDER_SHOE - 0.001, "foot_l", sock_top), (True, False))
        self.assertEqual(ws.foot_regions(top + 0.01, "calf_l", sock_top), (False, True))
        self.assertEqual(ws.foot_regions(sock_top, "calf_twist_02_l", sock_top), (False, False))  # 靴下の上端から上は肌
        self.assertEqual(ws.foot_regions(0.02, "thigh_l", sock_top), (False, False))     # すねと足の骨だけ
        self.assertEqual(ws.foot_regions(0.02, "ball_r", sock_top), (True, False))


if __name__ == "__main__":
    unittest.main()
