"""Blender で服を作る元の体の書き出しの規則（unreal に依存しない純粋なデータ。CI で単体テストする）。

Blender の build_players.py は、UE の Manny・Quinn のメッシュを FBX で読み、骨格と参照姿勢を変えずに服・髪・顔を足す（仮の選手、
ART-01 A-CHR-004・005）。build_metahuman_wear.py は、組み立てた MetaHuman の選手の体を読み、その殻としてテニスウェアを作る
（A-CHR-040〜043、ART-03 §6 の手順 5）。
書き出し先は Export/Base/。Export/ の直下ではないので、import_exported_fbx の対象にならない（import_rules）。
"""
from __future__ import annotations

from pathlib import PurePath

from .metahuman_players import PLAYERS as METAHUMAN_PLAYERS

BASE_SUBDIR = "Base"

# 書き出すメッシュの名前 → アセットのパス（Manny・Quinn は ART-01 A-CHR-002、MetaHuman の選手の体は A-CHR-010・011）
BASE_MESHES: dict[str, str] = {
    "SKM_Manny_Simple": "/Game/Tennis/Characters/Players/Mannequin/SKM_Manny_Simple",
    "SKM_Quinn_Simple": "/Game/Tennis/Characters/Players/Mannequin/SKM_Quinn_Simple",
    **{player.body_mesh: player.body_mesh_path for player in METAHUMAN_PLAYERS},
}


def base_fbx_path(export_dir: str, mesh_name: str) -> str:
    """元の体の FBX のパス（Export/Base/<名前>.fbx）。知らない名前なら KeyError。"""
    if mesh_name not in BASE_MESHES:
        raise KeyError(f"元の体の一覧にないメッシュ: {mesh_name}（BASE_MESHES に追加する）")
    return str(PurePath(export_dir) / BASE_SUBDIR / f"{mesh_name}.fbx")
