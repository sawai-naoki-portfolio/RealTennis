"""コートの面・余白・ラインを作る（A-CRT-001〜003）。何度実行しても同じ結果になる。

- SM_Court_Surface：余白の部分（ダブルスの区画を抜いた額縁）。z = 0
- SM_Court_Inner：ダブルスの区画。z = 0（Surface と重ならないので Z ファイティングがない）
- SM_Court_Lines：ライン 11 本を 1 メッシュに。面から LINE_Z だけ浮かせる
"""
from tennis_blender import bpy_utils as bu
from tennis_blender import layout
from tennis_blender import materials as mat

COLLECTION = "Tennis_Court"
LINE_Z = 0.001  # ラインを面から 1 mm 浮かせて Z ファイティングを防ぐ（ボールの物理の地面は z = 0 のまま）


def _flat_object(name, verts_xy, faces, z, collection, material_name):
    verts = [(x, y, z) for x, y in verts_xy]
    obj = bu.mesh_object(name, verts, faces, collection, material_name)
    bu.add_planar_uv(obj.data, axes=(0, 1))
    return obj


def build() -> None:
    coll = bu.reset_collection(COLLECTION)
    inner = layout.court_inner()
    lines = layout.court_lines()
    _flat_object("SM_Court_Surface", *layout.frame_to_quads(layout.court_surface(), inner),
                 0.0, coll, mat.COURT_OUT)
    _flat_object("SM_Court_Inner", *layout.rects_to_quads([inner]), 0.0, coll, mat.COURT_IN)
    _flat_object("SM_Court_Lines", *layout.rects_to_quads(lines), LINE_Z, coll, mat.COURT_LINE)
    bu.log(f"{COLLECTION}: SM_Court_Surface・SM_Court_Inner・SM_Court_Lines（ライン {len(lines)} 本）を作った")


if __name__ == "__main__":
    build()
