"""ネット・白帯・ポスト・シングルススティックを作る（A-NET-001・002）。何度実行しても同じ結果になる。

すべてコート座標（原点＝ネット中央の地面）で作る。ポストとスティックは、左右の 2 本を 1 つのメッシュにまとめる。
ネットと白帯の頂点カラー R は揺れの重み（UE の WPO で使う）。
"""
import bmesh
from mathutils import Matrix

from tennis_blender import bpy_utils as bu
from tennis_blender import dims as d
from tennis_blender import materials as mat
from tennis_blender import net_shape as ns

COLLECTION = "Tennis_Net"
CYLINDER_SEGMENTS = 16  # 4 の倍数にして、バウンディングボックスの幅が直径と一致するようにする
SWAY_ATTRIBUTE = "SwayWeight"


def _add_sway_weight(mesh) -> None:
    """頂点カラー（R＝揺れの重み、G・B＝0、A＝1）。リニアの値で持つ（FBX は colors_type='LINEAR' で書き出す）。"""
    attr = mesh.color_attributes.new(SWAY_ATTRIBUTE, 'FLOAT_COLOR', 'POINT')
    for vert, item in zip(mesh.vertices, attr.data):
        item.color = (ns.sway_weight(vert.co.y, vert.co.z), 0.0, 0.0, 1.0)
    mesh.color_attributes.active_color = attr
    mesh.color_attributes.render_color_index = mesh.color_attributes.active_color_index


def _cylinders(name, ys, diameter, height, collection, material_name):
    """X = 0 の各 y に、地面から立つ円柱を置いた 1 つのメッシュ。"""
    bm = bmesh.new()
    bm.loops.layers.uv.new("UVMap")  # calc_uvs は UV のレイヤーがあるときだけ働く
    for y in ys:
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=CYLINDER_SEGMENTS,
                              radius1=diameter / 2, radius2=diameter / 2, depth=height,
                              matrix=Matrix.Translation((0.0, y, height / 2)), calc_uvs=True)
    return bu.bmesh_object(name, bm, collection, material_name)


def build() -> None:
    coll = bu.reset_collection(COLLECTION)
    net = bu.mesh_object("SM_Net", *ns.net_grid(d.NET_SEGMENTS_Y, d.NET_SEGMENTS_Z), coll, mat.NET)
    band = bu.mesh_object("SM_NetBand", *ns.band(d.NET_SEGMENTS_Y), coll, mat.NET_BAND)
    for obj in (net, band):
        bu.add_planar_uv(obj.data, axes=(1, 2))
        _add_sway_weight(obj.data)
    _cylinders("SM_NetPost", (-d.POST_Y, d.POST_Y), d.POST_DIAMETER, d.POST_HEIGHT, coll, mat.NET_POST)
    _cylinders("SM_SinglesStick", (-d.STICK_Y, d.STICK_Y), d.STICK_DIAMETER, d.STICK_HEIGHT, coll, mat.NET_POST)
    bu.log(f"{COLLECTION}: SM_Net（{d.NET_SEGMENTS_Y} x {d.NET_SEGMENTS_Z}）・SM_NetBand・SM_NetPost・SM_SinglesStick を作った")


if __name__ == "__main__":
    build()
