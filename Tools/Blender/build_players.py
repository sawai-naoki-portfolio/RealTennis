"""仮の選手 2 人（ART-01 A-CHR-004・005：SKM_Player_Male01・SKM_Player_Female01）を作る。コレクション Tennis_Players。

入力は <書き出し先>/Base/ の Manny・Quinn の FBX（UE の TennisTools.export_player_base_fbx が書き出す）。書き出し先は
環境変数 TENNIS_EXPORT_DIR、なければリポジトリの Export/。骨格は UE から読んだまま（拡大 0.01）にし、export_all.py で
骨格ごと書き出す。仕様は tennis_blender/player_specs.py、形は head_shape・head_parts・player_regions。
"""
import os
from pathlib import Path

from tennis_blender import bpy_players as bp
from tennis_blender import bpy_utils as bu
from tennis_blender.player_specs import PLAYERS

COLLECTION = "Tennis_Players"


def base_dir() -> Path:
    env = os.environ.get("TENNIS_EXPORT_DIR")
    return (Path(env) if env else Path(__file__).resolve().parents[2] / "Export") / "Base"


def main() -> None:
    coll = bu.reset_collection(COLLECTION)
    for spec in PLAYERS:
        arm, body = bp.build_player(spec, base_dir(), coll)
        tris = bu.triangle_count(body)
        ok = tris <= spec.max_triangles
        bu.log(f"[{'OK' if ok else 'NG'}] {spec.name}：三角形 {tris:,}（上限 {spec.max_triangles:,}）、"
               f"骨 {len(arm.data.bones)}、マテリアル {len(body.data.materials)}")


if __name__ == "__main__":
    main()
