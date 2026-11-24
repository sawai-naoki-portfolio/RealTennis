"""スケルタルメッシュを FBX に書き出す（unreal に依存する。何をどこへ書き出すかの規則は player_base）。"""
import os

import unreal


def export_skeletal_mesh(asset_path: str, out_file: str) -> None:
    """LOD0 だけを、骨格（参照姿勢）・ウェイト・マテリアルのスロットごと書き出す。失敗したら RuntimeError。"""
    mesh = unreal.load_asset(asset_path)
    if not isinstance(mesh, unreal.SkeletalMesh):
        raise FileNotFoundError(f"Not a skeletal mesh: {asset_path}")
    os.makedirs(os.path.dirname(out_file), exist_ok=True)

    options = unreal.FbxExportOption()
    options.ascii = False
    options.level_of_detail = False  # LOD0 だけ（LOD は取り込んだ後に UE で作り直す）
    options.collision = False
    options.export_morph_targets = False
    options.vertex_color = True

    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = out_file
    task.exporter = unreal.SkeletalMeshExporterFBX()
    task.options = options
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    if not unreal.Exporter.run_asset_export_task(task) or not os.path.isfile(out_file):
        raise RuntimeError(f"FBX export failed: {asset_path} -> {out_file} {list(task.get_editor_property('errors'))}")
