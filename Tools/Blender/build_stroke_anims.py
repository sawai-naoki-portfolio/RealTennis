"""仮のストロークのアニメーション（フォア・バック）を作り、FBX に書き出す。コレクション Tennis_StrokeAnims。

入力は <書き出し先>/Base/SKM_Manny_Simple.fbx（UE の TennisTools.export_player_base_fbx が書き出す）。書き出し先は
環境変数 TENNIS_EXPORT_DIR、なければリポジトリの Export/。動きは tennis_blender/stroke_motion.py（その場で振る）。
出力は <書き出し先>/AS_*.fbx と、Notify の時刻を書いた AS_*.json（UE の側でモンタージュに使う）。
"""
import os
from pathlib import Path

import bpy

from tennis_blender import bpy_strokes as bs
from tennis_blender import bpy_utils as bu
from tennis_blender import stroke_motion as sm

COLLECTION = "Tennis_StrokeAnims"
RIG = "StrokeAnim_Rig"
BASE = "SKM_Manny_Simple"


def export_dir() -> Path:
    env = os.environ.get("TENNIS_EXPORT_DIR")
    return Path(env) if env else Path(__file__).resolve().parents[2] / "Export"


def main() -> None:
    if bpy.context.scene.unit_settings.system != 'METRIC':
        bu.log("エラー：単位が METRIC でない。作らない")
        return
    coll = bu.reset_collection(COLLECTION)
    arm, _ = bs.import_rig(export_dir() / "Base" / f"{BASE}.fbx", RIG, coll)
    for motion in sm.MOTIONS:
        bs.bake(arm, motion)
        bs.pose_frame(arm, sm.sample(motion, motion.impact_frame))
        spot = bs.sweet_spot(arm)
        bu.log(f"{motion.name}：インパクトのスイートスポット ({spot.x:.3f}, {spot.y:.3f}, {spot.z:.3f}) m")
        bs.export(arm, motion, export_dir())


if __name__ == "__main__":
    main()
