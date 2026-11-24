"""bpy の共通処理：コレクションの作り直し、マテリアル、メッシュのオブジェクトとソケットの作成、UV、ログ。"""
from __future__ import annotations

import math

import bpy
from mathutils import Matrix

from .materials import PREVIEW_COLORS
from .primitives import merge_slots

LOG_PREFIX = "[tennis_blender]"


def log(message: str) -> None:
    print(f"{LOG_PREFIX} {message}")


def reset_collection(name: str) -> bpy.types.Collection:
    """シーン直下のコレクション name を空にして返す（なければ作る）。中のオブジェクトと、使われなくなったメッシュを消す。"""
    coll = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    root = bpy.context.scene.collection
    if root.children.get(coll.name) is None:
        root.children.link(coll)
    removed = 0
    for obj in list(coll.objects):
        data = obj.data
        bpy.data.objects.remove(obj, do_unlink=True)
        if isinstance(data, bpy.types.Mesh) and data.users == 0:
            bpy.data.meshes.remove(data)
        removed += 1
    log(f"{name}: {removed} 個のオブジェクトを消して作り直す")
    return coll


def material(name: str) -> bpy.types.Material:
    """スロットのマテリアル。確認用の色（materials.PREVIEW_COLORS）を付ける。本番のシェーダーは UE で作る。

    同じ名前があれば使い回す。色は FBX の拡散色として UE に渡る。ノードは名前ではなく種類で探す（名前は翻訳される）。
    """
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    rgba = (*PREVIEW_COLORS[name], 1.0)  # 規則にない名前は KeyError（打ち間違いを早く見つける）
    mat.diffuse_color = rgba
    mat.use_nodes = True
    bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf is not None:
        bsdf.inputs["Base Color"].default_value = rgba
    return mat


def mesh_object(name, verts, faces, collection, material_name=None) -> bpy.types.Object:
    """頂点（m）と面からメッシュのオブジェクトを作る。トランスフォームは単位のまま。"""
    mesh = _new_mesh(name)
    mesh.from_pydata([tuple(v) for v in verts], [], [tuple(f) for f in faces])
    if mesh.validate(verbose=True):
        log(f"警告 {name}: 不正な形状を修正した（入力を見直す）")
    mesh.update()
    return _link(name, mesh, collection, material_name)


def bmesh_object(name, bm, collection, material_name=None) -> bpy.types.Object:
    """bmesh からメッシュのオブジェクトを作る（bm は解放する）。"""
    mesh = _new_mesh(name)
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()
    return _link(name, mesh, collection, material_name)


def set_materials(mesh: bpy.types.Mesh, names: list[str], face_indices: list[int]) -> None:
    """スロット names（確認用の色つき）を順に足し、面ごとにスロットの番号（names の位置）を入れる。"""
    if len(face_indices) != len(mesh.polygons):
        raise ValueError(f"{mesh.name}: 面の数 {len(mesh.polygons)} とスロットの番号の数 {len(face_indices)} が違う")
    for name in names:
        mesh.materials.append(material(name))
    mesh.polygons.foreach_set("material_index", face_indices)
    mesh.update()


def slotted_object(name: str, items, collection, sharp_angle_deg: float = 50.0) -> bpy.types.Object:
    """スロットつきの部品（(スロットの名前, (頂点, 面)) の並び）を 1 つのメッシュのオブジェクトにする。

    UV は箱の投影（m）。sharp_angle_deg より折れた辺を硬くし、ほかは滑らかにする（八角形の 45° は滑らか、端面・箱の 90° は硬い）。
    """
    verts, faces, names, indices = merge_slots(items)
    obj = mesh_object(name, verts, faces, collection)
    set_materials(obj.data, names, indices)
    add_box_uv(obj.data)
    obj.data.shade_smooth()
    obj.data.set_sharp_from_angle(angle=math.radians(sharp_angle_deg))
    return obj


def triangle_count(obj: bpy.types.Object) -> int:
    obj.data.calc_loop_triangles()
    return len(obj.data.loop_triangles)


def socket(name: str, parent: bpy.types.Object, location, collection, size: float = 0.03) -> bpy.types.Object:
    """ソケットの Empty（UE では名前から SOCKET_ を除いたソケットになる）。parent の子で、位置は parent の座標（m）、回転は単位。"""
    obj = _new_object(name, None)
    obj.empty_display_type = 'ARROWS'
    obj.empty_display_size = size
    collection.objects.link(obj)
    obj.parent = parent  # 親の逆行列は単位のまま（位置＝親の座標）
    obj.location = location
    return obj


def is_identity(matrix, tol: float = 1e-6) -> bool:
    """正方行列が単位行列であること。"""
    return all(abs(a - b) <= tol for row, ref in zip(matrix, Matrix.Identity(len(matrix))) for a, b in zip(row, ref))


def has_identity_transform(obj: bpy.types.Object, tol: float = 1e-6) -> bool:
    """ワールドのトランスフォームが単位（位置 0・回転 0・拡大 1）であること。エクスポートの前の Apply が要らない。"""
    return is_identity(obj.matrix_world, tol)


def add_planar_uv(mesh: bpy.types.Mesh, axes: tuple[int, int]) -> None:
    """UV ch0 に、2 つの軸の座標（m）をそのまま入れる。UE ではマテリアルの係数でタイリングする。"""
    uv = mesh.uv_layers.new(name="UVMap")
    for loop in mesh.loops:
        co = mesh.vertices[loop.vertex_index].co
        uv.data[loop.index].uv = (co[axes[0]], co[axes[1]])


def add_box_uv(mesh: bpy.types.Mesh) -> None:
    """UV ch0 に、面ごとに、法線にいちばん近い軸を除いた 2 つの軸の座標（m）を入れる（箱の投影）。"""
    uv = mesh.uv_layers.new(name="UVMap")
    for poly in mesh.polygons:
        drop = max(range(3), key=lambda i: abs(poly.normal[i]))
        a, b = (i for i in range(3) if i != drop)
        for li in poly.loop_indices:
            co = mesh.vertices[mesh.loops[li].vertex_index].co
            uv.data[li].uv = (co[a], co[b])


def _new_mesh(name: str) -> bpy.types.Mesh:
    old = bpy.data.meshes.get(name)
    if old is not None and old.users == 0:
        bpy.data.meshes.remove(old)  # 名前を空けて、データにも同じ名前を付ける
    return bpy.data.meshes.new(name)


def _link(name, mesh, collection, material_name) -> bpy.types.Object:
    if material_name:
        mesh.materials.append(material(material_name))
    try:
        obj = _new_object(name, mesh)
    except RuntimeError:
        bpy.data.meshes.remove(mesh)
        raise
    collection.objects.link(obj)
    return obj


def _new_object(name, data) -> bpy.types.Object:
    obj = bpy.data.objects.new(name, data)
    if obj.name != name:
        bpy.data.objects.remove(obj)
        raise RuntimeError(f"{name}: 同じ名前のオブジェクトが別のコレクションにある。消してから実行し直す")
    return obj
