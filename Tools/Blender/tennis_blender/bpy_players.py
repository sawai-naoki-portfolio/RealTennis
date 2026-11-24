"""仮の選手（ART-01 A-CHR-004・005）を Blender で組み立てる処理（bpy に依存する）。

形と分け方は純粋なモジュール（head_shape・head_parts・player_regions・player_specs）が決め、ここは Blender のデータに
写すだけにする。体（Manny・Quinn）の骨格・参照姿勢・ウェイトは変えない。服は体の面を外へずらした殻で、体のウェイトを
そのまま持つ。頭と首は作り直し、ウェイトは首の骨から頭の骨へなめらかに移す。
"""
from __future__ import annotations

from pathlib import Path

import bmesh
import bpy
from mathutils import Vector
from mathutils.kdtree import KDTree

from . import bpy_utils as bu
from . import head_parts as hp
from . import head_shape as hs
from . import materials as m
from . import mesh_ops as mo
from . import player_regions as pr
from . import skirt_shape as sk
from .player_specs import (SHOE_OFFSET, SHORTS_FLARE, SHORTS_OFFSET, SKIRT_FLARE, SKIRT_LENGTH, SKIRT_PLEATS, SOLE_TOP,
                           TOP_OFFSET, WRIST_OFFSET, PlayerSpec)

UV_NAME = "UVMap"
COLLAR_RADIUS, COLLAR_POINTS, COLLAR_SMOOTH = 0.008, 72, 20  # 襟のリブ（筒の半径 m・点の数・なめらかにする回数）
VOXEL = 0.008        # 体を作り直すボクセルの大きさ（m）。Manny・Quinn は 45 個の重なった部品なので、1 つの面にする
DRAPE_STEPS = 24     # 服と肌を平らにする回数（Manny の板の段差・ひざとひじの継ぎ目を消す）
SINK = 0.004         # 平らにするとき、元の体の面（服は面＋厚み）より内へ入ってよい深さ（板の出っ張りを削る）
Part = tuple[list, list, list[str], list[list[tuple[str, float]]]]  # (頂点, 面, 面のマテリアル, 頂点のウェイト)


def import_base(spec: PlayerSpec, base_dir: Path, collection: bpy.types.Collection):
    """元の体の FBX を読む。根の Empty を外し、骨格（拡大 0.01）とメッシュの名前を変える。(骨格, 体) を返す。
    spec は name と base を持つもの（PlayerSpec、MetaHuman の服の wear_specs.WearSpec）。"""
    path = base_dir / f"{spec.base}.fbx"
    if not path.is_file():
        raise FileNotFoundError(f"{path} がない。UE の TennisTools.export_player_base_fbx で書き出す")
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path), use_custom_normals=True, automatic_bone_orientation=False,
                             ignore_leaf_bones=False, use_anim=False)
    new = [o for o in bpy.data.objects if o not in before]
    arm = next(o for o in new if o.type == 'ARMATURE')
    body = next(o for o in new if o.type == 'MESH')
    world = arm.matrix_world.copy()
    arm.parent = None
    arm.matrix_world = world
    for obj in new:
        if obj.type == 'EMPTY':
            bpy.data.objects.remove(obj, do_unlink=True)
            continue
        for coll in list(obj.users_collection):
            coll.objects.unlink(obj)
        collection.objects.link(obj)
    arm.name = arm.data.name = f"{spec.name}_Rig"
    body.name = body.data.name = spec.name
    old_materials = list(body.data.materials)
    body.data.materials.clear()
    for mat in old_materials:
        if mat is not None and mat.users == 0:
            bpy.data.materials.remove(mat)
    body.data.uv_layers[0].name = UV_NAME
    return arm, body


def mesh_arrays(body: bpy.types.Object):
    """(頂点のワールドの位置, 法線, 面, いちばん重いウェイトの骨)。"""
    mw = body.matrix_world
    rot = mw.to_3x3().normalized()
    pos = [tuple(mw @ v.co) for v in body.data.vertices]
    nrm = [tuple((rot @ v.normal).normalized()) for v in body.data.vertices]
    faces = [tuple(p.vertices) for p in body.data.polygons]
    names = [g.name for g in body.vertex_groups]
    bones = [names[max(v.groups, key=lambda g: g.weight).group] if v.groups else "" for v in body.data.vertices]
    return pos, nrm, faces, bones


def bone_heads(arm: bpy.types.Object) -> dict[str, tuple[float, float, float]]:
    return {b.name: tuple(arm.matrix_world @ b.head_local) for b in arm.data.bones}


def head_anchor(spec: PlayerSpec, pos, bones, heads) -> tuple[float, float, float]:
    """頭の座標の原点（あご先の高さの耳の軸）。頭頂は元の頭のいちばん上から 6 mm 下、耳の軸は頭の骨の 12 mm 前。"""
    crown = max(p[2] for p, b in zip(pos, bones) if b == "head") - 0.006
    return 0.0, heads["head"][1] - 0.012, crown - hs.CROWN_Z * spec.head.height


def landmarks(spec: PlayerSpec, heads, anchor) -> pr.Landmarks:
    w, f, b, _, _ = hs.profile_at(-0.06 * spec.head.height, spec.head)
    return pr.Landmarks(bones=heads, neck_axis=(0.0, anchor[1] - (f - b) / 2), collar_z=heads["spine_05"][2])


def remesh(orig: bpy.types.Object, arm: bpy.types.Object, name: str, collection) -> bpy.types.Object:
    """元の体をボクセルで 1 つの面に作り直し、ウェイトを元の体から写して、骨格のモディファイアを付け直す。"""
    body = duplicate(orig, name, collection)
    body.modifiers.clear()
    body.vertex_groups.clear()
    mod = body.modifiers.new("Remesh", 'REMESH')
    mod.mode, mod.voxel_size, mod.adaptivity = 'VOXEL', VOXEL / body.matrix_world.to_scale()[0], 0.0
    with bpy.context.temp_override(object=body, active_object=body):
        bpy.ops.object.modifier_apply(modifier=mod.name)
    transfer_weights(orig, body)
    body.modifiers.new("Armature", 'ARMATURE').object = arm
    bu.add_box_uv(body.data)
    return body


def transfer_weights(src: bpy.types.Object, dst: bpy.types.Object, k: int = 4) -> None:
    """dst の頂点に、src の近い k 個の頂点のウェイトを距離の逆数で混ぜて写す（player_regions.blend_weights）。"""
    names = [g.name for g in src.vertex_groups]
    kd = KDTree(len(src.data.vertices))
    for i, v in enumerate(src.data.vertices):
        kd.insert(src.matrix_world @ v.co, i)
    kd.balance()
    src_w = [[(names[g.group], g.weight) for g in v.groups if g.weight > 0.0] for v in src.data.vertices]
    groups: dict[str, bpy.types.VertexGroup] = {}
    for i, v in enumerate(dst.data.vertices):
        hits = kd.find_n(dst.matrix_world @ v.co, k)
        for bone, w in mo.blend_weights([(d, src_w[j]) for _, j, d in hits]):
            if bone not in groups:
                groups[bone] = dst.vertex_groups.new(name=bone)
            groups[bone].add([i], w, 'REPLACE')


def set_positions(obj: bpy.types.Object, world_pos) -> None:
    inv = obj.matrix_world.inverted()
    flat = [c for p in world_pos for c in (inv @ Vector(p))]
    obj.data.vertices.foreach_set("co", flat)
    obj.data.update()


def keep_faces(obj: bpy.types.Object, keep: list[bool]) -> None:
    """keep が False の面と、面のなくなった頂点を消す（ウェイトと UV は残る）。"""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[f for f, k in zip(bm.faces, keep) if not k], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(obj.data)
    bm.free()


def assign_materials(obj: bpy.types.Object, face_materials: list[str]) -> None:
    names = sorted(set(face_materials))
    obj.data.materials.clear()
    for name in names:
        obj.data.materials.append(bu.material(name))
    obj.data.polygons.foreach_set("material_index", [names.index(n) for n in face_materials])


def duplicate(src: bpy.types.Object, name: str, collection: bpy.types.Collection) -> bpy.types.Object:
    obj = src.copy()
    obj.data = src.data.copy()
    obj.name = obj.data.name = name
    collection.objects.link(obj)
    return obj


def shell(body, name, collection, pos, nrm, faces, adj, mask, offsets, materials_of_face, sink=SINK, steps=DRAPE_STEPS):
    """体の mask の面を、法線の向きへ offsets だけずらした殻（服）。体の溝をまたぐように steps 回平らにする（体の面より
    sink まで内へ入ってよい）。"""
    moved = mo.drape(pos, nrm, faces, adj, mask, [o - sink for o in offsets], steps)
    obj = duplicate(body, name, collection)
    set_positions(obj, moved)
    keep = mo.faces_inside(faces, mask)
    assign_materials(obj, materials_of_face(moved))
    keep_faces(obj, keep)
    return obj, moved


def part_object(name: str, parts: list[Part], collection) -> bpy.types.Object:
    """新しく作る部品（頭・髪・耳・目・スカートなど）を 1 つのオブジェクトにする（ワールドの m、トランスフォームは単位）。"""
    verts, faces, mats, weights = [], [], [], []
    for v, f, mt, w in parts:
        base = len(verts)
        verts += v
        faces += [tuple(i + base for i in face) for face in f]
        mats += mt
        weights += w
    obj = bu.mesh_object(name, verts, faces, collection)
    bu.add_box_uv(obj.data)
    assign_materials(obj, mats)
    for i, ws in enumerate(weights):
        for bone, w in ws:
            group = obj.vertex_groups.get(bone) or obj.vertex_groups.new(name=bone)
            group.add([i], w, 'REPLACE')
    return obj


def collar(body, name, collection, faces, top_mask, top_pos, lm: pr.Landmarks, material: str) -> bpy.types.Object:
    """シャツの首の縁に沿ったリブ（筒）。縁の段々と、首の付け根とのすき間を覆う。ウェイトは体から写す。"""
    loops = mo.boundary_loops(faces, mo.faces_inside(faces, top_mask))
    neck = max(loops, key=lambda lp: sum(top_pos[i][2] for i in lp) / len(lp))
    ring = mo.resample_loop(mo.smooth_loop([top_pos[i] for i in neck], COLLAR_SMOOTH), COLLAR_POINTS)
    v, f = mo.loop_tube(ring, (lm.neck_axis[0], lm.neck_axis[1], 0.0), COLLAR_RADIUS)
    obj = bu.mesh_object(name, v, f, collection)
    bu.add_box_uv(obj.data)
    assign_materials(obj, [material] * len(f))
    transfer_weights(body, obj)
    return obj


def head_parts(spec: PlayerSpec, anchor) -> list[Part]:
    d = spec.head

    def moved(mesh):
        return [(x + anchor[0], y + anchor[1], z + anchor[2]) for x, y, z in mesh[0]], mesh[1]

    def rigid(mesh, material):
        v, f = moved(mesh)
        return v, f, [material] * len(f), [[("head", 1.0)]] * len(v)

    hv, hf, lips = hs.head_mesh(d)
    head = (moved((hv, hf))[0], hf, [m.PLAYER_LIPS if lip else spec.skin for lip in lips],
            [hs.neck_weights(z, d) for _, _, z in hv])
    parts = [head, rigid(hp.hair_mesh(spec.hair, d), spec.hair_color)]
    for side in (1, -1):
        eye, iris = hp.eye_meshes(side, d)
        parts += [rigid(hp.ear_mesh(side, d), spec.skin), rigid(eye, m.PLAYER_EYE), rigid(iris, m.PLAYER_IRIS),
                  rigid(hp.brow_mesh(side, d), spec.hair_color)]
    if spec.hair.ponytail:
        tail, tie = hp.ponytail_mesh(spec.hair, d)
        parts += [rigid(tail, spec.hair_color), rigid(tie, spec.accent)]
    if spec.outfit.headband:
        parts.append(rigid(hp.headband_mesh(spec.hair, d), spec.accent))
    return parts


def skirt_part(heads, pos, bones) -> Part:
    pelvis, thigh, calf = heads["pelvis"], heads["thigh_l"], heads["calf_l"]
    z_top = pelvis[2] + pr.WAIST_ABOVE_PELVIS - 0.01
    z_bottom = thigh[2] + (calf[2] - thigh[2]) * SKIRT_LENGTH
    body = [p for p, b in zip(pos, bones) if b == "pelvis" or b in pr.THIGH]
    rings = sk.skirt_rings(body, (0.0, pelvis[1]), z_top, z_bottom, rings=9, segments=64, clearance=0.010,
                           flare=SKIRT_FLARE, pleats=SKIRT_PLEATS)
    v, f = hp.loft(list(reversed(rings)))  # 下から上へつなぐと外を向く
    weights = [sk.skirt_weights(p[0], (z_top - p[2]) / (z_top - z_bottom)) for p in v]
    return v, f, [m.PLAYER_WHITE] * len(f), weights


def join(target: bpy.types.Object, others: list[bpy.types.Object]) -> None:
    bpy.ops.object.select_all(action='DESELECT')
    for obj in others:
        obj.select_set(True)
    target.select_set(True)
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.join()


def finish(body: bpy.types.Object, max_triangles: int) -> None:
    """独自の法線を消してなめらかにし、三角形を上限の 9 割まで減らす（Decimate を先頭にして適用）。"""
    with bpy.context.temp_override(object=body, active_object=body, selected_objects=[body]):
        bpy.ops.mesh.customdata_custom_splitnormals_clear()
    body.data.shade_smooth()
    tris = bu.triangle_count(body)
    target = int(max_triangles * 0.9)
    if tris > target:
        mod = body.modifiers.new("Decimate", 'DECIMATE')
        mod.ratio = target / tris
        with bpy.context.temp_override(object=body, active_object=body):
            bpy.ops.object.modifier_move_to_index(modifier=mod.name, index=0)
            bpy.ops.object.modifier_apply(modifier=mod.name)
    body.data.shade_smooth()


def hands(orig: bpy.types.Object, name: str, collection, material: str) -> bpy.types.Object:
    """元の体の手（手のひらと指）だけを残した複製。"""
    _, _, faces, bones = mesh_arrays(orig)
    obj = duplicate(orig, name, collection)
    assign_materials(obj, [material] * len(faces))
    keep_faces(obj, [all(pr.is_hand(bones[i]) for i in f) for f in faces])
    return obj


def build_player(spec: PlayerSpec, base_dir: Path, collection: bpy.types.Collection):
    arm, orig = import_base(spec, base_dir, collection)
    orig.name = orig.data.name = f"{spec.name}_Source"
    body = remesh(orig, arm, spec.name, collection)
    pos, nrm, faces, bones = mesh_arrays(body)
    heads = bone_heads(arm)
    anchor = head_anchor(spec, pos, bones, heads)
    lm = landmarks(spec, heads, anchor)
    tags = [pr.classify(p, b, lm, spec.outfit) for p, b in zip(pos, bones)]
    adj = mo.adjacency(len(pos), faces)
    mask = {k: [k in t for t in tags] for k in (pr.TOP, pr.BOTTOM, pr.SHOE, pr.SOCK, pr.WRIST, pr.REMOVE, pr.SMOOTH)}

    pieces = []
    thigh_t = [pr.segment_t(p, heads[f"thigh_{b[-1]}"], heads[f"calf_{b[-1]}"]) if b in pr.THIGH else 0.0
               for p, b in zip(pos, bones)]
    top, top_pos = shell(body, f"{spec.name}_Top", collection, pos, nrm, faces, adj, mask[pr.TOP],
                         [TOP_OFFSET] * len(pos), lambda moved: [spec.top] * len(faces))
    pieces.append(collar(body, f"{spec.name}_Collar", collection, faces, mask[pr.TOP], top_pos, lm, spec.top))
    shorts_off = [SHORTS_OFFSET + SHORTS_FLARE * max(0.0, (t - spec.outfit.shorts * 0.6) / (spec.outfit.shorts * 0.4))
                  for t in thigh_t]
    bottom, _ = shell(body, f"{spec.name}_Bottom", collection, pos, nrm, faces, adj, mask[pr.BOTTOM], shorts_off,
                      lambda moved: [m.PLAYER_WHITE] * len(faces))
    shoe, _ = shell(body, f"{spec.name}_Shoe", collection, pos, nrm, faces, adj, mask[pr.SHOE], [SHOE_OFFSET] * len(pos),
                    lambda moved: [m.PLAYER_SOLE if max(moved[i][2] for i in f) < SOLE_TOP else m.PLAYER_SHOE
                                   for f in faces])
    flatten_soles(shoe)
    pieces += [top, bottom, shoe]
    band, _ = shell(body, f"{spec.name}_Wrist", collection, pos, nrm, faces, adj, mask[pr.WRIST],
                    [WRIST_OFFSET] * len(pos), lambda moved: [spec.accent] * len(faces))
    pieces.append(band)

    # 1 つの服の殻の中に深く入っている体だけを消す（2 つの殻の境目の体は残す。穴を作らない）
    deep = [mo.erode(mask[k], adj, 3) for k in (pr.TOP, pr.BOTTOM, pr.SHOE)]
    hidden = [any(d[i] for d in deep) or mask[pr.REMOVE][i] or pr.is_hand(bones[i]) or
              (pr.TOP in tags[i] and pr.near_neck(pos[i], lm, spec.outfit.neck_dip, pr.HIDE_MARGIN))
              for i in range(len(pos))]
    set_positions(body, mo.drape(pos, nrm, faces, adj, mask[pr.SMOOTH], [-SINK] * len(pos), DRAPE_STEPS))
    assign_materials(body, [m.PLAYER_WHITE if all(mask[pr.SOCK][i] for i in f) else spec.skin for f in faces])
    keep_faces(body, [not all(hidden[i] for i in f) for f in faces])

    parts = head_parts(spec, anchor)
    if spec.outfit.skirt:
        parts.append(skirt_part(heads, pos, bones))
    pieces.append(part_object(f"{spec.name}_Head", parts, collection))
    pieces.append(hands(orig, f"{spec.name}_Hands", collection, spec.skin))
    source_mesh = orig.data
    bpy.data.objects.remove(orig, do_unlink=True)
    bpy.data.meshes.remove(source_mesh)
    join(body, pieces)
    finish(body, spec.max_triangles)
    return arm, body


def flatten_soles(shoe: bpy.types.Object) -> None:
    """靴の殻が床（z＝0）より下に出ないようにする（靴底は平ら）。"""
    mw = shoe.matrix_world
    inv = mw.inverted()
    for v in shoe.data.vertices:
        w = mw @ v.co
        if w.z < 0.0:
            v.co = inv @ Vector((w.x, w.y, 0.0))
    shoe.data.update()

