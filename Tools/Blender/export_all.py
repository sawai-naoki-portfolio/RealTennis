"""Tennis_* コレクションの SM_ のオブジェクトと、選手の SKM_ のメッシュ（骨格ごと）を、1 つずつ FBX に書き出す（<書き出し先>/<名前>.fbx）。

- 書き出し先：環境変数 TENNIS_EXPORT_DIR。なければリポジトリの Export/（Git の対象外）
- 軸：axis_forward='X'・axis_up='Z'（UE の +X 前・+Z 上に合わせる意図）。UE での確認は SM_AxisTest で行う（README §5.2）
- 単位：1 BU = 1 m を FBX_SCALE_UNITS で書き出す（UE では 100 cm になる）
- 子（UCX_・SOCKET_）も一緒に書き出す。トランスフォームが単位でないもの・隠れているものは書き出さずにエラーを出す
- .blend は保存しない。選択と作業中のオブジェクトは元に戻す
- SKM_（選手。build_players.py）：親の骨格（Armature）ごと書き出す。UE の Manny・Quinn を FBX で読んだ骨格は、
  UE の root の骨がオブジェクトになり、UE の cm のため拡大 0.01 を持つ。UE → Blender → UE の往復で骨と参照姿勢が
  変わらない設定（2026-11-12 に確かめた：骨 89 本、差は 0.0002 cm・0.0004°）で書き出す。骨格のオブジェクトの名前は
  UE の root の骨の名前になるので、書き出しの間だけ ROOT_BONE にする（2 人の骨格は同じ名前にできない）
"""
import os
from pathlib import Path

import bpy
from mathutils import Matrix

from tennis_blender import bpy_utils as bu

COLLECTION_PREFIX = "Tennis_"
FBX_OPTIONS = dict(
    use_selection=True,
    object_types={'MESH', 'EMPTY'},
    apply_unit_scale=True,
    apply_scale_options='FBX_SCALE_UNITS',
    axis_forward='X',
    axis_up='Z',
    mesh_smooth_type='FACE',
    colors_type='LINEAR',  # 頂点カラー（ネットの揺れの重み）をリニアの値のまま書き出す
    use_mesh_modifiers=True,
    add_leaf_bones=False,
    bake_anim=False,
)
ROOT_BONE = "root"  # UE の骨格 SK_TennisPlayer_Base の根の骨
SKELETAL_FBX_OPTIONS = dict(
    FBX_OPTIONS,
    object_types={'ARMATURE', 'MESH'},
    apply_scale_options='FBX_SCALE_NONE',  # 骨格の拡大 0.01 × 単位 100 ＝ 1（UE の cm に戻る）
    primary_bone_axis='Y',                 # 読み込み（import_scene.fbx の既定）と同じ軸にして、骨の向きを元に戻す
    secondary_bone_axis='X',
    armature_nodetype='NULL',
    use_armature_deform_only=False,        # 骨格の骨をすべて書き出す（UE の骨格と同じ骨の数にする）
)
UE_CM = 0.01  # 骨格のオブジェクトの拡大（UE の cm を Blender の m に読んだもの）


def export_dir() -> Path:
    env = os.environ.get("TENNIS_EXPORT_DIR")
    return Path(env) if env else Path(__file__).resolve().parents[2] / "Export"


def _targets() -> list:
    scene_colls = set(bpy.context.scene.collection.children_recursive)
    objects = [o for c in bpy.data.collections if c.name.startswith(COLLECTION_PREFIX) and c in scene_colls for o in c.objects]
    static = [o for o in objects if o.name.startswith("SM_") and o.parent is None]
    skeletal = [o for o in objects if o.name.startswith("SKM_") and o.type == 'MESH']
    return sorted(static + skeletal, key=lambda o: o.name)


def _problem(obj) -> str | None:
    if not obj.visible_get():
        return "隠れている（表示してから実行する）"
    if obj.name.startswith("SKM_"):
        return _skeletal_problem(obj)
    if not bu.has_identity_transform(obj):
        return "トランスフォームが単位でない（頂点を直接その位置に作る）"
    return None


def _skeletal_problem(obj) -> str | None:
    arm = obj.parent
    if arm is None or arm.type != 'ARMATURE':
        return "親が骨格（Armature）でない"
    if not arm.visible_get():
        return f"骨格 {arm.name} が隠れている（選べないので骨が書き出されない。表示してから実行する）"
    if not bu.is_identity(arm.matrix_world @ Matrix.Scale(1.0 / UE_CM, 4)):
        return f"骨格のトランスフォームが、拡大 {UE_CM} だけでない（UE から読んだままにする）"
    if not bu.is_identity(obj.matrix_local):
        return "骨格に対するメッシュのトランスフォームが単位でない"
    return None


def _export(obj, path: Path) -> None:
    bpy.ops.object.select_all(action='DESELECT')
    if obj.name.startswith("SKM_"):
        _export_skeletal(obj, path)
        return
    for o in [obj, *obj.children_recursive]:
        o.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(path), **FBX_OPTIONS)


def _export_skeletal(obj, path: Path) -> None:
    arm = obj.parent
    arm.select_set(True)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = arm
    name, other = arm.name, bpy.data.objects.get(ROOT_BONE)
    if other is not None and other is not arm:
        other.name = ROOT_BONE + "__export_tmp"
    arm.name = ROOT_BONE
    try:
        bpy.ops.export_scene.fbx(filepath=str(path), **SKELETAL_FBX_OPTIONS)
    finally:
        arm.name = name
        if other is not None and other is not arm:
            other.name = ROOT_BONE


def main() -> None:
    units = bpy.context.scene.unit_settings
    if units.system != 'METRIC' or abs(units.scale_length - 1.0) > 1e-9:
        bu.log(f"エラー：単位が METRIC・Unit Scale 1.0 でない（{units.system}、{units.scale_length}）。書き出さない")
        return
    if bpy.context.mode != 'OBJECT':
        bu.log(f"エラー：オブジェクトモードでない（{bpy.context.mode}）。書き出さない")
        return
    out = export_dir()
    out.mkdir(parents=True, exist_ok=True)
    selected = list(bpy.context.selected_objects)
    active = bpy.context.view_layer.objects.active
    targets = _targets()
    errors = 0 if targets else 1
    if not targets:
        bu.log(f"[NG] 書き出すものがない（{COLLECTION_PREFIX}* のコレクションに SM_ のオブジェクトがない。build_*.py を先に実行する）")
    try:
        for obj in targets:
            problem = _problem(obj)
            if problem:
                bu.log(f"[NG] {obj.name}：{problem}")
                errors += 1
                continue
            path = out / f"{obj.name}.fbx"
            _export(obj, path)
            bu.log(f"[OK] {obj.name} → {path}（{path.stat().st_size:,} バイト）")
    finally:
        bpy.ops.object.select_all(action='DESELECT')
        for o in selected:
            o.select_set(True)
        bpy.context.view_layer.objects.active = active
    bu.log(f"export_all：{'OK' if errors == 0 else 'NG'}（エラー {errors} 件、書き出し先 {out}）")


if __name__ == "__main__":
    main()
