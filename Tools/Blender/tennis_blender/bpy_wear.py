"""MetaHuman の選手のテニスウェア（ART-01 A-CHR-040〜043）の組み立て（bpy に依存する）。

形と分け方は純粋なモジュール（wear_specs・player_regions・player_specs）が決め、殻の作り方は仮の選手と同じ（bpy_players）。
組み立てた体を読み、UV の継ぎ目で分かれた頂点をつないで殻の元にし、服の殻（シャツ・ショートパンツ・靴下・リストバンド）と
襟のリブ、スカート、靴（足の凸包）を 1 つのメッシュにする。体そのものは書き出さない（UE では体の上に重ねて描く）。
ウェイトは、殻は体の頂点から 1 対 1、靴・襟・スカートは近くの体の頂点または決まった骨から。骨格と参照姿勢は変えない。
"""
from __future__ import annotations

from pathlib import Path

import bmesh
import bpy
from mathutils import Vector

from . import bpy_players as bp
from . import bpy_utils as bu
from . import materials as m
from . import mesh_ops as mo
from . import player_regions as pr
from . import wear_specs as ws
from .player_specs import SHOE_OFFSET, SHORTS_FLARE, SHORTS_OFFSET, SOLE_TOP, TOP_OFFSET, WRIST_OFFSET


def build_wear(spec: ws.WearSpec, base_dir: Path, collection: bpy.types.Collection):
    """spec の服を作る。(骨格, 服) を返す（服の名前は spec.name、骨格は <spec.name>_Rig）。"""
    arm, body = bp.import_base(spec, base_dir, collection)
    body.name = body.data.name = f"{spec.name}_Body"
    _weld(body)
    pos, nrm, faces, heaviest = bp.mesh_arrays(body)
    parent_of = {b.name: (b.parent.name if b.parent else None) for b in arm.data.bones}
    bones = [pr.manny_bone(b, parent_of) for b in heaviest]
    heads = bp.bone_heads(arm)
    # 体の FBX は首まで（頭は顔のメッシュ）なので、目印は骨の位置から決める
    lm = pr.Landmarks(bones=heads, neck_axis=(heads["neck_01"][0], heads["neck_01"][1]), collar_z=heads["spine_05"][2],
                      collar_r=ws.COLLAR_R)
    outfit = spec.outfit
    tags = [pr.classify(p, b, lm, outfit) for p, b in zip(pos, bones)]
    feet = [ws.foot_regions(p[2], b, outfit.sock_top) for p, b in zip(pos, bones)]
    adj = mo.adjacency(len(pos), faces)

    def shell(name, region, offsets, material):
        return bp.shell(body, name, collection, pos, nrm, faces, adj, region, offsets, lambda moved: [material] * len(faces),
                        sink=ws.SINK, steps=ws.DRAPE_STEPS)

    top_mask = [pr.TOP in t for t in tags]
    wear, top_pos = shell(spec.name, top_mask, [TOP_OFFSET] * len(pos), spec.top)
    pieces = [bp.collar(body, f"{spec.name}_Collar", collection, faces, top_mask, top_pos, lm, spec.top)]
    thigh_t = [pr.segment_t(p, heads[f"thigh_{b[-1]}"], heads[f"calf_{b[-1]}"]) if b in pr.THIGH else 0.0
               for p, b in zip(pos, bones)]
    shorts_off = [SHORTS_OFFSET + SHORTS_FLARE * max(0.0, (t - outfit.shorts * 0.6) / (outfit.shorts * 0.4)) for t in thigh_t]
    pieces.append(shell(f"{spec.name}_Bottom", [pr.BOTTOM in t for t in tags], shorts_off, m.PLAYER_WHITE)[0])
    pieces.append(shell(f"{spec.name}_Sock", [f[1] for f in feet], [ws.SOCK_OFFSET] * len(pos), m.PLAYER_WHITE)[0])
    pieces.append(shell(f"{spec.name}_Wrist", [pr.WRIST in t for t in tags], [WRIST_OFFSET] * len(pos), spec.accent)[0])
    if outfit.skirt:
        pieces.append(bp.part_object(f"{spec.name}_Skirt", [bp.skirt_part(heads, pos, bones)], collection))
    for side in ("l", "r"):
        points = [Vector(p) + Vector(n) * SHOE_OFFSET for p, n, f, b in zip(pos, nrm, feet, bones) if f[0] and b.endswith("_" + side)]
        pieces.append(_shoe(body, f"{spec.name}_Shoe_{side}", collection, points))

    bp.join(wear, pieces)
    with bpy.context.temp_override(object=wear, active_object=wear, selected_objects=[wear]):
        bpy.ops.mesh.customdata_custom_splitnormals_clear()
    wear.data.shade_smooth()
    if bu.is_identity(wear.matrix_local, ws.TRANSFORM_NOISE):
        # FBX の読み込みで、骨格に対するトランスフォームにごく小さな誤差が付くことがある（F01 で 2e-6 cm）。書き出しは単位を求める
        wear.matrix_parent_inverse.identity()
        wear.matrix_basis.identity()
        bpy.context.view_layer.update()  # matrix_local はワールドの行列から求まるので、更新してから書き出しが調べる
    source = body.data
    bpy.data.objects.remove(body, do_unlink=True)
    bpy.data.meshes.remove(source)
    return arm, wear


def _weld(body: bpy.types.Object) -> None:
    """UV の継ぎ目で分かれた頂点をつなぐ（分かれたままだと継ぎ目の両側で法線が違い、殻が継ぎ目で割れて肌が見える）。
    ウェイトと UV（面の角ごと）は残る。読んだ独自の法線は形に合わなくなるので消す。"""
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=ws.WELD_DISTANCE / body.matrix_world.to_scale()[0])
    bm.to_mesh(body.data)
    bm.free()
    with bpy.context.temp_override(object=body, active_object=body, selected_objects=[body]):
        bpy.ops.mesh.customdata_custom_splitnormals_clear()


def _shoe(body: bpy.types.Object, name: str, collection, points: list[Vector]) -> bpy.types.Object:
    """足の頂点を外へずらした点の凸包を、均等な面に作り直して三角形を減らした靴（指を 1 つの形にまとめる）。
    靴底は平ら（z＝0）。ウェイトは近くの体の頂点から写す。"""
    bm = bmesh.new()
    for p in points:
        bm.verts.new(p)
    bmesh.ops.convex_hull(bm, input=list(bm.verts))
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    obj = bu.bmesh_object(name, bm, collection)  # bm はここで解放される
    remesh = obj.modifiers.new("Remesh", 'REMESH')
    remesh.mode, remesh.voxel_size, remesh.adaptivity = 'VOXEL', ws.SHOE_VOXEL, 0.0
    _apply(obj, remesh)
    decimate = obj.modifiers.new("Decimate", 'DECIMATE')
    decimate.ratio = ws.SHOE_TRIANGLES / max(1, bu.triangle_count(obj))
    _apply(obj, decimate)
    bp.flatten_soles(obj)
    mw = obj.matrix_world
    bp.assign_materials(obj, [m.PLAYER_SOLE if max((mw @ obj.data.vertices[i].co).z for i in poly.vertices) < SOLE_TOP
                              else m.PLAYER_SHOE for poly in obj.data.polygons])
    bu.add_box_uv(obj.data)
    bp.transfer_weights(body, obj)
    return obj


def _apply(obj: bpy.types.Object, mod: bpy.types.Modifier) -> None:
    with bpy.context.temp_override(object=obj, active_object=obj):
        bpy.ops.object.modifier_apply(modifier=mod.name)
