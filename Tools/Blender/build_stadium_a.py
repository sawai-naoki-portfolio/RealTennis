"""センターコート A のスタジアムの仮の形を作る（A-STD-010）：SM_Stadium_A_Stands・SM_Stadium_A_Stands_LOD1・
SM_Stadium_A_Barrier・SM_Stadium_A_Floor。

すべてコート局所座標（原点＝ネット中央の地面）。形は tennis_blender/stadium_a.py。何度実行しても同じ結果になる。
LOD1 は LOD0 と同じ位置に重なる（確認の画像を撮るときは LOD1 を一時的に隠し、書き出しの前に戻す）。
"""
from tennis_blender import bpy_utils as bu
from tennis_blender import stadium_a as sa

COLLECTION = "Tennis_StadiumA"


def build() -> None:
    coll = bu.reset_collection(COLLECTION)
    made = [bu.slotted_object(name, items, coll) for name, items in sa.parts().items()]
    bu.log(f"{COLLECTION}: " + "・".join(f"{o.name}（三角形 {bu.triangle_count(o)}）" for o in made)
           + f" を作った（席の見積もり {sa.seat_estimate():,}）")


if __name__ == "__main__":
    build()
