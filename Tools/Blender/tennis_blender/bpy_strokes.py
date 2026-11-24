"""仮のストロークの動き（stroke_motion）を、UE の Manny の骨格に焼いて FBX に書き出す処理（bpy に依存する）。

姿勢は stroke_motion が決め、ここは骨へ写すだけにする。フレームごとに、腰 → 背骨 → 首 → 脚（足は地面に固定）→ 腕の順に
骨の向きを決め、全フレームにキーを打つ。骨格は UE から読んだまま（オブジェクトが UE の root の骨。拡大 0.01、単位は cm）。
"""
from __future__ import annotations

import json
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector

from . import bpy_utils as bu
from . import stroke_motion as sm

SPINE = ("spine_01", "spine_02", "spine_03", "spine_04", "spine_05")
NECK = ("neck_01", "neck_02", "head")
NECK_COUNTER = 0.9            # 首と頭で、体のひねりのこの割合を戻す（顔を前に向けておく）
CM = 100.0                    # 選手の座標（m）→ 骨格の座標（cm）
KNEE_POLE = Vector((0.0, -1.0, 0.15))
ELBOW_POLE = {"r": Vector((-0.25, 0.55, -0.80)), "l": Vector((0.25, 0.55, -0.80))}
FBX_OPTIONS = dict(
    use_selection=True, object_types={'ARMATURE'}, apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
    axis_forward='X', axis_up='Z', add_leaf_bones=False, primary_bone_axis='Y', secondary_bone_axis='X',
    armature_nodetype='NULL', use_armature_deform_only=False,
    bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
    bake_anim_force_startend_keying=True, bake_anim_step=1.0, bake_anim_simplify_factor=0.0,
)
ROOT_BONE = "root"  # UE の骨格の根の骨（書き出しの間だけ、骨格のオブジェクトをこの名前にする。export_all.py と同じ）


def import_rig(base_fbx: Path, name: str, collection: bpy.types.Collection):
    """元の体（Manny）の FBX を読み、(骨格, メッシュ) を返す。メッシュは姿勢を目で確かめるためだけに置く。"""
    if not base_fbx.is_file():
        raise FileNotFoundError(f"{base_fbx} がない。UE の TennisTools.export_player_base_fbx で書き出す")
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(base_fbx), use_custom_normals=True, automatic_bone_orientation=False,
                             ignore_leaf_bones=False, use_anim=False)
    new = [o for o in bpy.data.objects if o not in before]
    arm = next(o for o in new if o.type == 'ARMATURE')
    mesh = next(o for o in new if o.type == 'MESH')
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
    arm.name = arm.data.name = name
    mesh.name = mesh.data.name = f"{name}_Body"
    return arm, mesh


def _update() -> None:
    bpy.context.view_layer.update()


def _yaw_basis(bone: bpy.types.Bone, degrees: float) -> Matrix:
    """参照姿勢の上向き（親と一緒に回る）のまわりに回す、骨の局所の回転。"""
    rest = bone.matrix_local.to_3x3()
    return (rest.inverted() @ Matrix.Rotation(degrees * 3.141592653589793 / 180.0, 3, 'Z') @ rest).to_4x4()


def _aim(pose_bone, child, direction: Vector) -> None:
    """骨の付け根を動かさずに、子の付け根が direction の向きに来るように回す（最短の回転）。"""
    head = pose_bone.head.copy()
    turn = (child.head - head).rotation_difference(direction).to_matrix().to_4x4()
    pose_bone.matrix = Matrix.Translation(head) @ turn @ Matrix.Translation(-head) @ pose_bone.matrix
    _update()


def _limb(pose, upper: str, lower: str, end: str, target: Vector, pole: Vector) -> None:
    """2 本の骨の IK：upper の付け根から、end の付け根が target（骨格の座標、cm）に来るように曲げる。"""
    rest = pose.id_data.data.bones
    length_upper = (rest[lower].head_local - rest[upper].head_local).length
    length_lower = (rest[end].head_local - rest[lower].head_local).length
    root = pose.bones[upper].head.copy()
    joint, tip = sm.two_bone_ik(tuple(root), tuple(target), length_upper, length_lower, tuple(pole))
    _aim(pose.bones[upper], pose.bones[lower], Vector(joint) - root)
    _aim(pose.bones[lower], pose.bones[end], Vector(tip) - pose.bones[lower].head)


def _set_rotation(pose_bone, rotation: Matrix) -> None:
    """骨の付け根の位置はそのままに、骨格の座標での向きを決める。"""
    pose_bone.matrix = Matrix.Translation(pose_bone.head.copy()) @ rotation.to_4x4()
    _update()


def _hand_rotation(arm, key: sm.StrokeKey) -> Matrix:
    """ラケット（ソケット racket_attach_r）がキーの向きになる、hand_r の骨格の座標での向き。"""
    rest = arm.data.bones["hand_r"].matrix_local.to_3x3()
    axis, normal = rest @ Vector(sm.SOCKET_RACKET_AXIS_IN_HAND), rest @ Vector(sm.SOCKET_FACE_NORMAL_IN_HAND)
    rest_frame = Matrix((axis, normal.cross(axis), normal)).transposed()
    x, y, z = sm.racket_frame(key.racket_dir, key.face_normal)
    want_frame = Matrix((x, y, z)).transposed()
    return want_frame @ rest_frame.transposed() @ rest


def pose_frame(arm, key: sm.StrokeKey) -> None:
    """1 フレームの姿勢を骨に当てる（キーは打たない）。"""
    pose, rest = arm.pose, arm.data.bones
    for pose_bone in pose.bones:
        pose_bone.rotation_mode = 'QUATERNION'
        pose_bone.matrix_basis = Matrix.Identity(4)
    pelvis = rest["pelvis"]
    head = pelvis.head_local
    pose.bones["pelvis"].matrix = (Matrix.Translation((0.0, 0.0, -key.pelvis_drop * CM)) @ Matrix.Translation(head)
                                   @ Matrix.Rotation(key.pelvis_yaw * 3.141592653589793 / 180.0, 4, 'Z')
                                   @ Matrix.Translation(-head) @ pelvis.matrix_local)
    for name in SPINE:
        pose.bones[name].matrix_basis = _yaw_basis(rest[name], key.chest_yaw / len(SPINE))
    for name in NECK:
        pose.bones[name].matrix_basis = _yaw_basis(rest[name], -(key.pelvis_yaw + key.chest_yaw) * NECK_COUNTER / len(NECK))
    _update()

    for side in ("l", "r"):  # 足は参照姿勢の位置と向きのまま（地面に固定）
        _limb(pose, f"thigh_{side}", f"calf_{side}", f"foot_{side}", rest[f"foot_{side}"].head_local, KNEE_POLE)
        _set_rotation(pose.bones[f"foot_{side}"], rest[f"foot_{side}"].matrix_local.to_3x3())

    hand = _hand_rotation(arm, key)
    wrist = Vector(sm.grip_position(key)) * CM - hand @ (Vector(sm.SOCKET_LOCATION_IN_HAND) * CM)
    _limb(pose, "upperarm_r", "lowerarm_r", "hand_r", wrist, ELBOW_POLE["r"])
    _set_rotation(pose.bones["hand_r"], hand)
    _limb(pose, "upperarm_l", "lowerarm_l", "hand_l", Vector(key.off_wrist) * CM, ELBOW_POLE["l"])


def sweet_spot(arm) -> Vector:
    """今の姿勢でのスイートスポット（選手の座標、m）。確かめるために使う。"""
    hand = arm.pose.bones["hand_r"]
    rotation = hand.matrix.to_3x3()
    grip = hand.head + rotation @ (Vector(sm.SOCKET_LOCATION_IN_HAND) * CM)
    return (grip + rotation @ Vector(sm.SOCKET_RACKET_AXIS_IN_HAND) * sm.GRIP_TO_SWEET_SPOT * CM) / CM


def bake(arm, motion: sm.StrokeMotion) -> bpy.types.Action:
    """全フレームの姿勢を焼いて、motion.name のアクションにする。四元数は前のフレームと同じ側にそろえる。"""
    old = bpy.data.actions.get(motion.name)
    if old is not None:
        bpy.data.actions.remove(old)
    if arm.animation_data is not None:
        arm.animation_data_clear()
    previous: dict[str, Quaternion] = {}
    for frame in range(motion.last_frame + 1):
        pose_frame(arm, sm.sample(motion, frame))
        for pose_bone in arm.pose.bones:
            rotation = pose_bone.rotation_quaternion.copy()
            if pose_bone.name in previous and previous[pose_bone.name].dot(rotation) < 0.0:
                rotation.negate()
                pose_bone.rotation_quaternion = rotation
            previous[pose_bone.name] = rotation
            pose_bone.keyframe_insert("rotation_quaternion", frame=frame)
        arm.pose.bones["pelvis"].keyframe_insert("location", frame=frame)
    action = arm.animation_data.action
    action.name = motion.name
    return action


def export(arm, motion: sm.StrokeMotion, out_dir: Path) -> Path:
    """アクションを FBX（骨格とアニメーションだけ）に書き出し、隣に Notify の時刻の JSON を置く。"""
    scene = bpy.context.scene
    scene.render.fps, scene.frame_start, scene.frame_end = sm.FPS, 0, motion.last_frame
    out_dir.mkdir(parents=True, exist_ok=True)
    path = out_dir / f"{motion.name}.fbx"
    bpy.ops.object.select_all(action='DESELECT')
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    name = arm.name
    arm.name = ROOT_BONE
    try:
        bpy.ops.export_scene.fbx(filepath=str(path), **FBX_OPTIONS)
    finally:
        arm.name = name
    path.with_suffix(".json").write_text(json.dumps(sm.notify_times(motion), indent=2) + "\n", encoding="utf-8")
    bu.log(f"[OK] {motion.name} → {path}（{motion.last_frame + 1} フレーム、インパクト {motion.impact_frame}）")
    return path
