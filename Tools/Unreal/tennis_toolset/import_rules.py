"""FBX の取り込みの規則（unreal に依存しない純粋な関数。CI で単体テストする）。

- 取り込める元：プロジェクトの Export/ の直下にある .fbx だけ（パスの指定は受け付けない。ファイル名だけ）
- 取り込み先：アセット名の接頭辞から決める（ART-03 §5 のフォルダ構成）。呼び出し側は選べない
- Nanite：アセットの種類ごとに決める（ART-01 §1 の予算表。コート・ネット・ボール・ラケット・小物は使わない。NFR-MOB-002）
- コリジョン：アセットの種類ごとに決める（ART-03 §5）。ラケットと小物（練習コート・コートサイド）には付けない（Interchange が自動で作る、
  全体を覆う形を消す。フェンスの形は中のコート全体を覆い、選手が中に出現して落ちる）
- 仮のアセット：Export/ の直下のファイルから作られたもの。取り込み直すたびに FBX の値へ戻してよい
- LOD：スタジアム（SM_Stadium_）の _LOD<n> は、元のメッシュの LOD n に入れる（別のアセットを残さない）
- 選手（SKM_Player_）：既存の骨格 SK_TennisPlayer_Base に取り込む（新しい骨格・物理アセットを作らない。骨格の参照姿勢を
  変えない）。LOD は UE で作る（ART-01 §1 の予算：選手は 4。NFR-MOB-002）
- MetaHuman の選手のテニスウェア（SKM_Wear_）：組み立てた体の共通の骨格 metahuman_base_skel に取り込む（ART-03 §6 の手順 5）。
  体に追従する部品（Leader Pose）なので物理アセットは作らず付けない。LOD は体と同じ 3 つ
- ストロークのアニメーション（AS_FH_・AS_BH1_）：既存の骨格 SK_TennisPlayer_Base に、アニメーションだけを取り込む（メッシュ・
  骨格を作らない）。名前の種類は ART-02 §1。ロコモーションの AS_*（Locomotion/）は Export/ から取り込まないので、規則に入れない
"""
from __future__ import annotations

import os
import re
from collections.abc import Iterable
from dataclasses import dataclass
from pathlib import PurePath

from . import metahuman_players

ART_ROOT = "/Game/Tennis/Art"
CHARACTERS_ROOT = "/Game/Tennis/Characters"

# アセット名の接頭辞 → (取り込み先のフォルダ, Nanite を使うか, コリジョンを残すか)（上から順に照合する）
DESTINATIONS: tuple[tuple[str, str, bool, bool], ...] = (
    ("SM_Court_", f"{ART_ROOT}/Court", False, True),     # 選手が立つ地面
    ("SM_Net", f"{ART_ROOT}/Net", False, True),          # WPO で揺らす。Nanite を使わない
    ("SM_SinglesStick", f"{ART_ROOT}/Net", False, True),
    ("SM_Ball", f"{ART_ROOT}/Ball", False, True),
    ("SM_Racket", f"{ART_ROOT}/Racket", False, False),     # 選手の手に付ける。三角形が少ない（ART-01 §2）。打球は距離で決める
    ("SM_Practice_", f"{ART_ROOT}/Stadium", False, False),  # 練習コートの小物（A-STD-001）。計 1,408 三角形で Nanite は要らない
    ("SM_Stadium_", f"{ART_ROOT}/Stadium", False, False),   # スタジアムの仮の形（A-STD-010）。自動の凸包は観客席の輪の全体を覆い、選手が落ちる
    ("SM_Prop_", f"{ART_ROOT}/Props", False, False),       # コートサイドの小物（A-PRP-001〜006）。計 648 三角形。位置と向きはアクタで決める
    ("SM_AxisTest", f"{ART_ROOT}/Dev", False, True),
)


@dataclass(frozen=True)
class SkeletalSettings:
    skeleton: str        # 取り込み先の骨格（既存。作らない）
    physics_asset: str | None  # 使う物理アセット（既存。作らない）。None は付けない（体に追従する部品）
    lod_count: int       # UE で作る LOD の数（LOD0 を含む）


PLAYER_SKELETAL = SkeletalSettings(
    skeleton=f"{CHARACTERS_ROOT}/Base/SK_TennisPlayer_Base",
    physics_asset=f"{CHARACTERS_ROOT}/Players/Mannequin/PA_Mannequin",
    lod_count=4,
)

WEAR_SKELETAL = SkeletalSettings(
    skeleton=metahuman_players.SKELETON,
    physics_asset=None,  # 体に追従する部品。当たりと物理は体のものを使う
    lod_count=3,         # 組み立てた体（Optimized・Medium）と同じ
)

@dataclass(frozen=True)
class AnimationSettings:
    skeleton: str        # 取り込み先の骨格（既存。作らない）


PLAYER_ANIMATION = AnimationSettings(skeleton=PLAYER_SKELETAL.skeleton)

# アニメーションのアセット名の接頭辞 → (取り込み先のフォルダ, 設定)。ストロークの種類（ART-02 §1）のうち、今あるものだけ
ANIMATION_DESTINATIONS: tuple[tuple[str, str, AnimationSettings], ...] = (
    ("AS_FH_", f"{CHARACTERS_ROOT}/Animations/Strokes", PLAYER_ANIMATION),   # 仮のフォアハンド（W-3.4）
    ("AS_BH1_", f"{CHARACTERS_ROOT}/Animations/Strokes", PLAYER_ANIMATION),  # 仮の片手のバックハンド（W-3.4）
)

# スケルタルメッシュのアセット名の接頭辞 → (取り込み先のフォルダ, 設定)
SKELETAL_DESTINATIONS: tuple[tuple[str, str, SkeletalSettings], ...] = (
    ("SKM_Player_", f"{CHARACTERS_ROOT}/Players/Interim", PLAYER_SKELETAL),  # 仮の選手（ART-01 A-CHR-004・005）
    ("SKM_Wear_", metahuman_players.WEAR_FOLDER, WEAR_SKELETAL),             # MetaHuman の選手のテニスウェア（A-CHR-040〜043）
)

# この接頭辞のアセットは、名前が _LOD<n> で終わるものを、別のアセットにせず、元のメッシュの LOD n に入れる（ART-03 §5）。
# ボール（SM_Ball_LOD1）は別のアセットのまま（遠くのボールに差し替えて使う）
LOD_MERGE_PREFIXES: tuple[str, ...] = ("SM_Stadium_", "SM_Prop_A_")
_LOD_SUFFIX = re.compile(r"^(?P<base>.+)_LOD(?P<index>[1-7])$")

_NAME = re.compile(r"^[A-Za-z0-9_]+\.fbx$")


class ImportRuleError(ValueError):
    """取り込みの規則に合わない要求。"""


@dataclass(frozen=True)
class ImportPlan:
    source_file: str        # 絶対パス（Export/ の下）
    destination_path: str   # /Game/Tennis/Art/...
    asset_name: str
    nanite: bool
    collision: bool         # False なら、取り込みで付いた単純コリジョンを消す
    skeletal: SkeletalSettings | None = None  # スケルタルメッシュなら設定、スタティックメッシュなら None
    lod_of: str | None = None  # 元のメッシュのアセット名。None でなければ、取り込んだ後に元のメッシュの LOD lod_index に入れて、自分は消す
    lod_index: int = 0
    animation: AnimationSettings | None = None  # アニメーションなら設定（メッシュは取り込まない）


def plan_import(file_name: str, export_dir: str) -> ImportPlan:
    """ファイル名を検証し、取り込みの計画を返す。規則に合わなければ ImportRuleError。"""
    if not _NAME.fullmatch(file_name):
        raise ImportRuleError(f"ファイル名だけを指定する（英数字と _、拡張子 .fbx）: {file_name!r}")
    asset_name = file_name[: -len(".fbx")]
    source = str(PurePath(export_dir) / file_name)
    for prefix, destination, animation in ANIMATION_DESTINATIONS:
        if asset_name.startswith(prefix):
            return ImportPlan(source, destination, asset_name, nanite=False, collision=True, animation=animation)
    for prefix, destination, skeletal in SKELETAL_DESTINATIONS:
        if asset_name.startswith(prefix):
            return ImportPlan(source, destination, asset_name, nanite=False, collision=True, skeletal=skeletal)
    for prefix, destination, nanite, collision in DESTINATIONS:
        if asset_name.startswith(prefix):
            lod = _LOD_SUFFIX.fullmatch(asset_name) if asset_name.startswith(LOD_MERGE_PREFIXES) else None
            return ImportPlan(source, destination, asset_name, nanite, collision,
                              lod_of=lod["base"] if lod else None, lod_index=int(lod["index"]) if lod else 0)
    raise ImportRuleError(f"取り込み先の規則がないアセット名: {asset_name}（DESTINATIONS に追加する）")


def is_from_export(source_files: Iterable[str], export_dir: str) -> bool:
    """アセットの取り込み元に Export/ の直下のファイルがあれば True（そのアセットは仮のもの）。

    UE で作った本番のアセット（取り込み元がない、または別の場所）は False になり、取り込みのツールは触れない。
    """
    export = _normalized(export_dir)
    return any(_normalized(os.path.dirname(_normalized(f))) == export for f in source_files if f)


def _normalized(path: str) -> str:
    return os.path.normcase(os.path.normpath(path.replace("\\", "/")))
