"""練習コートの小物を作る（A-STD-001）：SM_Practice_Fence・SM_Practice_Bench・SM_Practice_LightPole。

すべてコート局所座標（原点＝ネット中央の地面）。形は tennis_blender/practice_court.py。何度実行しても同じ結果になる。
"""
from tennis_blender import bpy_utils as bu
from tennis_blender import practice_court as pc

COLLECTION = "Tennis_Practice"


def build() -> None:
    coll = bu.reset_collection(COLLECTION)
    made = [bu.slotted_object(name, items, coll) for name, items in pc.parts().items()]
    bu.log(f"{COLLECTION}: " + "・".join(f"{o.name}（三角形 {bu.triangle_count(o)}）" for o in made) + " を作った")


if __name__ == "__main__":
    build()
