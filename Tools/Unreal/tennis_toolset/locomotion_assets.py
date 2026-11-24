"""ロコモーションの Motion Matching のアセットの場所と、M3 の仮のデータのアニメーションの一覧（unreal に依存しない。CI で単体テストする）。

スキーマ PSS_Locomotion とデータベース PSD_Locomotion は、build_locomotion_database が作る（DD-03 §6.5）。仮のデータは、テンプレートの
待機と、歩き・ジョグの 8 方向ずつ（ART-01 A-CHR-003。骨格 SK_TennisPlayer_Base）。
"""
from __future__ import annotations

from .stroke_assets import OWNER_TAG  # ツールが作ったアセットの目印（ツールの間で同じ名前）

LOCOMOTION_FOLDER = "/Game/Tennis/Characters/Animations/Locomotion"
SCHEMA_NAME = "PSS_Locomotion"
DATABASE_NAME = "PSD_Locomotion"
SCHEMA_PATH = f"{LOCOMOTION_FOLDER}/{SCHEMA_NAME}"
DATABASE_PATH = f"{LOCOMOTION_FOLDER}/{DATABASE_NAME}"
SKELETON_PATH = "/Game/Tennis/Characters/Base/SK_TennisPlayer_Base"

OWNER_VALUE = "build_locomotion_database"

DIRECTIONS = ("Fwd", "Fwd_Left", "Left", "Bwd_Left", "Bwd", "Bwd_Right", "Right", "Fwd_Right")

# 仮のデータ（LOCOMOTION_FOLDER からの相対）：待機と、歩き・ジョグの 8 方向ずつ
PLACEHOLDER_ANIMATIONS: tuple[str, ...] = (
    ("Mannequin/MM_Idle",)
    + tuple(f"Mannequin/Walk/MF_Unarmed_Walk_{direction}" for direction in DIRECTIONS)
    + tuple(f"Mannequin/Jog/MF_Unarmed_Jog_{direction}" for direction in DIRECTIONS)
)


def animation_paths() -> list[str]:
    """データベースに入れるアニメーションのパス（この順に入れる）。"""
    return [f"{LOCOMOTION_FOLDER}/{name}" for name in PLACEHOLDER_ANIMATIONS]
