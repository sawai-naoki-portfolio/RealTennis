"""MetaHuman の選手のテニスウェア（ART-01 A-CHR-040〜043、ART-03 §6 の手順 5）の仕様（純粋なデータ。bpy に依存しない）。

服は、組み立てた体（Export/Base/SKM_MHC_Player_<key>_BodyMesh.fbx。UE の TennisTools.export_player_base_fbx が書き出す）の殻
として作る。服装と色は仮の選手と同じ（player_specs の Outfit とマテリアル）。体の面は消さない（UE では体の上に重ねて描く）ので、
殻は体の面より内へ入れない。靴は足の凸包（指を 1 つの形にまとめる）、靴下は靴の上端より少し下から重ねる。
ヘッドバンドは頭（顔のメッシュ）の形が要るので、ここでは作らない（後に足す：ART-03 §6）。
"""
from __future__ import annotations

from dataclasses import dataclass, replace

from .player_regions import CALF, FOOT
from .player_specs import FEMALE01, MALE01, SHOE_TOP, Outfit


@dataclass(frozen=True)
class WearSpec:
    name: str      # UE のアセット名（SKM_Wear_<key>。取り込みの規則は Tools/Unreal の import_rules）
    base: str      # 組み立てた体（Export/Base/<base>.fbx）
    outfit: Outfit
    top: str       # シャツ・トップのマテリアルのスロット（materials.py）
    accent: str    # リストバンド


WEAR_M01 = WearSpec("SKM_Wear_M01", "SKM_MHC_Player_M01_BodyMesh", MALE01.outfit, MALE01.top, MALE01.accent)
WEAR_F01 = WearSpec("SKM_Wear_F01", "SKM_MHC_Player_F01_BodyMesh", replace(FEMALE01.outfit, headband=False),
                    FEMALE01.top, FEMALE01.accent)
WEARS: tuple[WearSpec, ...] = (WEAR_M01, WEAR_F01)

# 襟ぐりの後ろの半径（m。首の軸から）。MetaHuman の体は首に大きな穴があり（顔のメッシュの首がふさぐ。後ろの縁は首の軸から
# M01 8.4 cm・F01 5.7 cm：2026-11-18）、その縁をそのまま襟にする。仮の選手の 7.8 cm で切ると、F01 の背中の体（組み立ての
# 灰色の下着が焼き込まれている）が見えた。前は Outfit.neck_dip だけ広げる
COLLAR_R = 0.05
DRAPE_STEPS = 8          # 殻を平らにする回数（MetaHuman の体はなめらかなので、仮の選手より少ない）
SINK = 0.0               # 殻が体の面より内へ入ってよい深さ（m）。体を消さないので 0（入ると肌が出る）
SOCK_OFFSET = 0.003      # 靴下の厚み（m）
SOCK_UNDER_SHOE = 0.025  # 靴下を靴の上端より下へ重ねる幅（m。境目に隙間を作らない）
SHOE_VOXEL = 0.005       # 靴の凸包を均等な面に作り直す大きさ（m）
SHOE_TRIANGLES = 1200    # 片足の靴の三角形
WELD_DISTANCE = 1e-5     # UV の継ぎ目で分かれた頂点をつなぐ距離（m）
TRANSFORM_NOISE = 1e-4   # 骨格に対するメッシュのトランスフォームを単位とみなす誤差（骨格の単位：cm）


def foot_regions(z: float, bone: str, sock_top: float) -> tuple[bool, bool]:
    """(靴, 靴下)。bone はいちばん重いウェイトの骨を Manny の骨にしたもの（player_regions.manny_bone）、z は高さ（m）。
    すねと足の頂点を高さで分ける。靴下は靴の上端より SOCK_UNDER_SHOE 下から sock_top まで（靴と重なってよい）。"""
    lower = bone in CALF or bone in FOOT
    return lower and z < SHOE_TOP, lower and SHOE_TOP - SOCK_UNDER_SHOE <= z < sock_top
