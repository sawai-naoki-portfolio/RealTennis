"""MetaHuman の選手のテニスウェア（ART-01 A-CHR-040〜043）：SKM_Wear_M01・SKM_Wear_F01 と、その骨格（*_Rig）を作る。コレクション Tennis_Wear。

入力は <書き出し先>/Base/ の組み立てた体の FBX（UE の TennisTools.export_player_base_fbx が書き出す）。書き出し先は環境変数
TENNIS_EXPORT_DIR、なければリポジトリの Export/。export_all.py で骨格ごと書き出し、UE の import_exported_fbx で取り込み、
apply_metahuman_wear で選手の見た目のデータに入れる（ART-03 §6 の手順 5・6）。仕様は tennis_blender/wear_specs.py。
"""
from build_players import base_dir
from tennis_blender import bpy_utils as bu
from tennis_blender import bpy_wear as bw
from tennis_blender.wear_specs import WEARS

COLLECTION = "Tennis_Wear"


def main() -> None:
    coll = bu.reset_collection(COLLECTION)
    for spec in WEARS:
        arm, wear = bw.build_wear(spec, base_dir(), coll)
        bu.log(f"{spec.name}：三角形 {bu.triangle_count(wear):,}、骨 {len(arm.data.bones)}、マテリアル {len(wear.data.materials)}")


if __name__ == "__main__":
    main()
