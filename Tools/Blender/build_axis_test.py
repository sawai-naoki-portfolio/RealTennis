"""FBX の軸の確認用の矢印 SM_AxisTest を作る。何度実行しても同じ結果になる。

長さで向きを見分ける：+X は矢じりつきの 1.0 m、+Y は 0.4 m の棒、+Z は 0.2 m の柱。
UE に取り込み、原点に置いて、それぞれが UE の +X（赤）・+Y（緑）・+Z（青）のどちらを向くかを見る（README §5.2）。
"""
import bmesh
from mathutils import Matrix

from tennis_blender import bpy_utils as bu

COLLECTION = "Tennis_AxisTest"
T = 0.05  # 棒の太さ（m）


def _box(bm, x0, x1, y0, y1, z0, z1):
    center = ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
    scale = Matrix.Diagonal((x1 - x0, y1 - y0, z1 - z0, 1.0))
    bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Translation(center) @ scale)


def build() -> None:
    coll = bu.reset_collection(COLLECTION)
    bm = bmesh.new()
    h = T / 2
    _box(bm, 0.0, 0.8, -h, h, 0.0, T)          # +X の軸
    _box(bm, -h, h, 0.0, 0.4, 0.0, T)          # +Y の棒
    _box(bm, -h, h, -h, h, 0.0, 0.2)           # +Z の柱
    head = Matrix.Translation((0.9, 0.0, h)) @ Matrix.Rotation(1.5707963267948966, 4, 'Y')
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=4, radius1=0.08, radius2=0.0,
                          depth=0.2, matrix=head)  # +X の矢じり（先端 x = 1.0）
    bu.bmesh_object("SM_AxisTest", bm, coll)
    bu.log(f"{COLLECTION}: SM_AxisTest（+X 1.0 m、+Y 0.4 m、+Z 0.2 m）を作った")


if __name__ == "__main__":
    build()
