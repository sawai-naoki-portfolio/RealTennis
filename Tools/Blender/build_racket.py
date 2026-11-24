"""ラケットを作る（A-RKT-001）：SM_Racket と、子のソケット SOCKET_racket_sweetspot・SOCKET_grip（Empty）。

局所座標は原点＝グリップエンド、+X＝先端、Y＝フェイスの幅、Z＝フェイスの法線（形は tennis_blender/racket_shape.py）。
マテリアルのスロットはグリップ・フレーム・ストリングの 3 つ。何度実行しても同じ結果になる。
"""
from tennis_blender import bpy_utils as bu
from tennis_blender import racket_shape as rs

COLLECTION = "Tennis_Racket"


def build() -> None:
    coll = bu.reset_collection(COLLECTION)
    racket = bu.slotted_object("SM_Racket", rs.parts(), coll)
    for name, location in rs.sockets().items():
        bu.socket(name, racket, location, coll)
    slots = "・".join(m.name for m in racket.data.materials)
    bu.log(f"{COLLECTION}: SM_Racket（三角形 {bu.triangle_count(racket)}、スロット {slots}）と {'・'.join(rs.sockets())} を作った")


if __name__ == "__main__":
    build()
