"""Export/ の FBX を取り込む処理（unreal に依存する。何をどこへ取り込むかの規則は import_rules）。

UE 5.8 の Interchange の既定の動きのうち、規則に合わない 4 つをここで直す：

- スタティックメッシュを Nanite を有効にして取り込む → 規則（ImportPlan.nanite）に合わせる
- UCX_ のコリジョンがないメッシュに、全体を覆う単純コリジョンを自動で付ける → 規則（ImportPlan.collision）が
  False のアセットからは消す（フェンスの形は中のコート全体を覆い、選手が中に出現して落ちる）
- ソケット（SOCKET_ の Empty）に、FBX の軸と単位の変換の逆を掛ける。export_all.py の書き出し（X 前・Z 上・
  FBX_SCALE_UNITS）では、位置は正しいが、回転 90°・スケール 100 が残る → 回転 0・スケール 1（メッシュの軸のまま。
  ART-03 §3）に戻す。Blender の側の Empty の回転が単位であることは validate_dimensions.py が確かめている
- 既存のアセットへ取り込み直すと、マテリアルには触れない（再取り込みの方針の既定が APPLY_NO_PROPERTIES。
  方針を変えても、メッシュの再取り込みではマテリアルを更新しない）。そのため Blender で変えた確認用の色が反映されない
  → 一時フォルダーへ新しく取り込み、そのパラメーターの値を、Export/ から作られた既存の仮のマテリアルへ写す

LOD 用のメッシュ（ImportPlan.lod_of）は、取り込んだ後に元のメッシュの LOD に入れて、LOD 用のアセットを消す。

スケルタルメッシュ（ImportPlan.skeletal）は、既定では新しい骨格と物理アセットを作る → 既存の骨格と物理アセットを
指定し、骨格の参照姿勢を変えない。LOD は取り込んだ後に UE の削減で作る。

アニメーション（ImportPlan.animation）も、既定では FBX の骨格とメッシュを新しく作る → 既存の骨格を指定し、
アニメーションだけを取り込む。マテリアルはないので、仮のマテリアルの合わせ直しはしない。
"""
import unreal

from .import_rules import AnimationSettings, ImportPlan, SkeletalSettings, is_from_export

TEMP_ROOT = "/Game/Tennis/_ImportTemp"  # 取り込みのあいだだけ使い、最後に必ず消す
_PARAMETER_PROPERTIES = ("scalar_parameter_values", "vector_parameter_values", "texture_parameter_values")


def import_plans(plans: list[ImportPlan], export_dir: str) -> list[str]:
    """計画どおりに取り込み、保存する。取り込んだアセットのオブジェクトパスを返す。"""
    imported: list[str] = []
    for plan, paths in zip(plans, _run_tasks(plans, [p.destination_path for p in plans], save=True)):
        _apply_nanite(paths, plan.nanite)
        if not plan.collision:
            _remove_collision(paths)
        _reset_socket_axes(paths)
        if plan.skeletal is not None:
            _check_skeleton_and_build_lods(paths, plan.skeletal)
        if plan.animation is not None:
            _check_animation_skeleton(paths, plan.animation)
        imported.extend(paths)
    _refresh_preview_materials([plan for plan in plans if plan.animation is None], export_dir)
    for plan in plans:
        if plan.lod_of is not None:
            imported.remove(_merge_lod(plan))
    return imported


def _merge_lod(plan: ImportPlan) -> str:
    """取り込んだ LOD 用のメッシュを、元のメッシュの LOD に入れて保存し、LOD 用のアセットを消す。消したオブジェクトパスを返す。"""
    base_path = f"{plan.destination_path}/{plan.lod_of}.{plan.lod_of}"
    lod_path = f"{plan.destination_path}/{plan.asset_name}.{plan.asset_name}"
    base, lod = unreal.load_asset(base_path), unreal.load_asset(lod_path)
    if not isinstance(base, unreal.StaticMesh) or not isinstance(lod, unreal.StaticMesh):
        raise FileNotFoundError(f"LOD merge needs both meshes: {base_path} (import it first), {lod_path}")
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if subsystem.set_lod_from_static_mesh(base, plan.lod_index, lod, 0, True) != plan.lod_index:
        raise RuntimeError(f"Could not set LOD {plan.lod_index} of {base_path} from {lod_path}")
    unreal.EditorAssetLibrary.save_loaded_asset(base, only_if_is_dirty=False)
    if not unreal.EditorAssetLibrary.delete_asset(lod_path.split(".")[0]):
        raise RuntimeError(f"Could not delete the merged LOD asset {lod_path}")
    unreal.log(f"[TennisTools] {base_path}: LOD {plan.lod_index} from {plan.asset_name} ({subsystem.get_lod_count(base)} LODs)")
    return lod_path


def _run_tasks(plans: list[ImportPlan], destinations: list[str], save: bool) -> list[list[str]]:
    results: dict[int, list[str]] = {}
    static = [(i, plan, destination) for i, (plan, destination) in enumerate(zip(plans, destinations))
              if plan.skeletal is None and plan.animation is None]
    for i, (plan, destination) in enumerate(zip(plans, destinations)):
        if plan.skeletal is not None:
            results[i] = _import_interchange(plan, destination, save, _skeletal_pipeline(plan.skeletal), unreal.SkeletalMesh)
        elif plan.animation is not None:
            results[i] = _import_interchange(plan, destination, save, _animation_pipeline(plan.animation), unreal.AnimSequence)
    for (i, _, _), paths in zip(static, _run_static_tasks([p for _, p, _ in static], [d for _, _, d in static], save)):
        results[i] = paths
    return [results[i] for i in range(len(plans))]


def _run_static_tasks(plans: list[ImportPlan], destinations: list[str], save: bool) -> list[list[str]]:
    if not plans:
        return []
    tasks = []
    for plan, destination in zip(plans, destinations):
        task = unreal.AssetImportTask()
        task.filename = plan.source_file
        task.destination_path = destination
        task.destination_name = plan.asset_name
        task.replace_existing = True
        task.automated = True
        task.save = save
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    results = []
    for plan, task in zip(plans, tasks):
        paths = list(task.get_editor_property("imported_object_paths"))
        if not paths:
            raise RuntimeError(f"Import produced no assets: {plan.source_file}")
        results.append(paths)
    return results


def _import_interchange(plan: ImportPlan, destination: str, save: bool,
                        pipeline: unreal.InterchangeGenericAssetsPipeline, expected: type) -> list[str]:
    """AssetImportTask は Interchange のパイプラインの設定を受け取らないので、Interchange を直接呼ぶ。"""
    pipeline.set_editor_property("asset_name", plan.asset_name)
    params = unreal.ImportAssetParameters()
    params.set_editor_property("is_automated", True)
    params.set_editor_property("replace_existing", True)
    params.set_editor_property("override_pipelines", [unreal.SoftObjectPath(pipeline.get_path_name())])
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    objects = manager.import_asset(destination, unreal.InterchangeManager.create_source_data(plan.source_file), params) or []
    paths = [obj.get_path_name() for obj in objects]
    if not any(isinstance(obj, expected) for obj in objects):
        raise RuntimeError(f"Import produced no {expected.__name__}: {plan.source_file} -> {paths}")
    if save:
        for obj in objects:
            unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False)
    return paths


def _skeletal_pipeline(settings: SkeletalSettings) -> unreal.InterchangeGenericAssetsPipeline:
    """既存の骨格と物理アセットに取り込む設定（骨格の参照姿勢・アニメーション・モーフは取り込まない）。物理アセットの指定が
    None なら、作らず付けない（体に追従する部品）。"""
    skeleton = unreal.load_asset(settings.skeleton)
    physics_asset = unreal.load_asset(settings.physics_asset) if settings.physics_asset is not None else None
    if not isinstance(skeleton, unreal.Skeleton) or (
            settings.physics_asset is not None and not isinstance(physics_asset, unreal.PhysicsAsset)):
        raise FileNotFoundError(f"Skeleton or physics asset not found: {settings.skeleton}, {settings.physics_asset}")
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    common = pipeline.get_editor_property("common_skeletal_meshes_and_animations_properties")
    common.set_editor_property("skeleton", skeleton)
    common.set_editor_property("use_t0_as_ref_pose", False)
    mesh = pipeline.get_editor_property("mesh_pipeline")
    mesh.set_editor_property("import_static_meshes", False)
    mesh.set_editor_property("import_skeletal_meshes", True)
    mesh.set_editor_property("import_morph_targets", False)
    mesh.set_editor_property("update_skeleton_reference_pose", False)
    mesh.set_editor_property("create_physics_asset", False)
    if physics_asset is not None:
        mesh.set_editor_property("physics_asset", physics_asset)
    pipeline.get_editor_property("animation_pipeline").set_editor_property("import_animations", False)
    return pipeline


def _animation_pipeline(settings: AnimationSettings) -> unreal.InterchangeGenericAssetsPipeline:
    """既存の骨格に、アニメーションだけを取り込む設定（メッシュ・骨格・物理アセットを作らない）。"""
    skeleton = unreal.load_asset(settings.skeleton)
    if not isinstance(skeleton, unreal.Skeleton):
        raise FileNotFoundError(f"Skeleton not found: {settings.skeleton}")
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    common = pipeline.get_editor_property("common_skeletal_meshes_and_animations_properties")
    common.set_editor_property("skeleton", skeleton)
    common.set_editor_property("import_only_animations", True)
    mesh = pipeline.get_editor_property("mesh_pipeline")
    mesh.set_editor_property("import_static_meshes", False)
    mesh.set_editor_property("import_skeletal_meshes", False)
    pipeline.get_editor_property("animation_pipeline").set_editor_property("import_animations", True)
    return pipeline


def _check_animation_skeleton(object_paths: list[str], settings: AnimationSettings) -> None:
    """取り込んだアニメーションが指定の骨格に入ったことを確かめる（違えば止める）。"""
    for path in object_paths:
        sequence = unreal.load_asset(path)
        if not isinstance(sequence, unreal.AnimSequence):
            continue
        skeleton = sequence.get_editor_property("skeleton")
        if skeleton is None or skeleton.get_path_name().split(".")[0] != settings.skeleton:
            raise RuntimeError(f"{path} was imported with skeleton {skeleton}, expected {settings.skeleton}")
        unreal.log(f"[TennisTools] {path}: skeleton {settings.skeleton}, {sequence.get_play_length():.3f} s")


def _check_skeleton_and_build_lods(object_paths: list[str], settings: SkeletalSettings) -> None:
    """指定の骨格に入ったことを確かめ（違えば止める）、LOD を作り直して保存する。"""
    subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    for path in object_paths:
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.SkeletalMesh):
            continue
        skeleton = mesh.get_editor_property("skeleton")
        if skeleton is None or skeleton.get_path_name().split(".")[0] != settings.skeleton:
            raise RuntimeError(f"{path} was imported with skeleton {skeleton}, expected {settings.skeleton}")
        if not subsystem.regenerate_lod(mesh, settings.lod_count, True, False):
            raise RuntimeError(f"Could not build {settings.lod_count} LODs for {path}")
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
        unreal.log(f"[TennisTools] {path}: skeleton {settings.skeleton}, {subsystem.get_lod_count(mesh)} LODs")


def _apply_nanite(object_paths: list[str], enabled: bool) -> None:
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for path in object_paths:
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        settings = subsystem.get_nanite_settings(mesh)
        if settings.enabled != enabled:
            settings.enabled = enabled
            subsystem.set_nanite_settings(mesh, settings)
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)


def _remove_collision(object_paths: list[str]) -> None:
    """取り込みで付いた単純コリジョンを消す（複雑コリジョン＝見た目の三角形はそのまま）。"""
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for path in object_paths:
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        # get_simple_collision_count は箱・球・カプセルだけを数える。Interchange が自動で付けるのは凸包（18-DOP）
        if subsystem.get_simple_collision_count(mesh) + subsystem.get_convex_collision_count(mesh) > 0:
            subsystem.remove_collisions(mesh)
            unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
            unreal.log(f"[TennisTools] removed the simple collision of {path}")


def _reset_socket_axes(object_paths: list[str]) -> None:
    """取り込んだソケットの回転を 0、スケールを 1 にする（位置は FBX のまま）。"""
    no_rotation, unit_scale = unreal.Rotator(0.0, 0.0, 0.0), unreal.Vector(1.0, 1.0, 1.0)
    for path in object_paths:
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        changed = [socket for socket in mesh.get_sockets_by_tag("")  # 取り込みで作られたソケットにタグはない
                   if not (socket.relative_rotation.is_near_equal(no_rotation, 1.0e-4)
                           and socket.relative_scale.is_near_equal(unit_scale, 1.0e-6))]
        for socket in changed:
            socket.set_editor_property("relative_rotation", no_rotation)
            socket.set_editor_property("relative_scale", unit_scale)
            unreal.log(f"[TennisTools] reset the axes of socket {socket.socket_name} on {path}")
        if changed:
            unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)


def _refresh_preview_materials(plans: list[ImportPlan], export_dir: str) -> None:
    library = unreal.EditorAssetLibrary
    try:
        temp_folders = [f"{TEMP_ROOT}/{plan.asset_name}" for plan in plans]
        for plan, folder, paths in zip(plans, temp_folders, _run_tasks(plans, temp_folders, save=False)):
            for path in paths:
                fresh = unreal.load_asset(path)
                if not isinstance(fresh, unreal.MaterialInstanceConstant):
                    continue
                name = fresh.get_name()
                target = unreal.load_asset(f"{plan.destination_path}/{name}.{name}")
                if _is_preview_material(target, export_dir):
                    _copy_parameters(fresh, target)
                    library.save_loaded_asset(target)
                    unreal.log(f"[TennisTools] refreshed preview material {target.get_path_name()}")
    finally:
        if library.does_directory_exist(TEMP_ROOT) and not library.delete_directory(TEMP_ROOT):
            unreal.log_error(f"[TennisTools] could not delete {TEMP_ROOT}; delete it by hand (do not commit it)")


def is_export_asset(asset: object, export_dir: str) -> bool:
    """Export/ の直下の FBX から取り込まれたアセットなら True（仮のもの。取り込み直してよい）。"""
    import_data = asset.get_editor_property("asset_import_data") if asset is not None else None
    return import_data is not None and is_from_export(list(import_data.extract_filenames()), export_dir)


def _is_preview_material(material: object, export_dir: str) -> bool:
    """Export/ の FBX から作られた MaterialInstanceConstant だけを仮のものとして扱う（UE で作った本番のものには触れない）。"""
    return isinstance(material, unreal.MaterialInstanceConstant) and is_export_asset(material, export_dir)


def _copy_parameters(source: unreal.MaterialInstanceConstant, target: unreal.MaterialInstanceConstant) -> None:
    unreal.MaterialEditingLibrary.set_material_instance_parent(target, source.get_editor_property("parent"))
    for prop in _PARAMETER_PROPERTIES:
        target.set_editor_property(prop, source.get_editor_property(prop))
    unreal.MaterialEditingLibrary.update_material_instance(target)
