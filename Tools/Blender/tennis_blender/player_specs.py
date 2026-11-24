"""仮の選手 2 人（ART-01 A-CHR-004・005）の仕様（純粋なデータ。bpy に依存しない）。

体は UE の Manny・Quinn（Export/Base/ の FBX）を使い、骨格と参照姿勢は変えない。長さの割合は骨から次の骨までの間
（上腕：upperarm → lowerarm、太もも：thigh → calf）の位置で、0 が付け根。高さは m（足の裏が 0）。
"""
from __future__ import annotations

from dataclasses import dataclass

from . import materials as m
from .head_parts import SHORT, TIED, HairStyle
from .head_shape import HeadDims


@dataclass(frozen=True)
class Outfit:
    sleeve: float | None   # シャツの袖の長さ（上腕の割合）。None は袖なし
    neck_dip: float        # 襟ぐりの前の広がり（m。首の軸からの半径に足す）
    hem: float             # シャツの裾の高さ（骨盤の骨の高さからの差、m。負は下）
    skirt: bool            # スカート（下にショートパンツを重ねる）
    shorts: float          # ショートパンツの丈（太ももの割合）
    sock_top: float        # 靴下の上端の高さ（m）
    headband: bool


@dataclass(frozen=True)
class PlayerSpec:
    name: str              # UE のアセット名（SKM_Player_*）
    base: str              # 元の体（Export/Base/<base>.fbx）
    head: HeadDims
    hair: HairStyle
    outfit: Outfit
    skin: str              # マテリアルのスロット（materials.py）
    hair_color: str
    top: str
    accent: str            # リストバンド・ヘッドバンド・髪のゴム
    max_triangles: int = 60_000  # ART-01 §1：選手（体＋服）の LOD0


MALE01 = PlayerSpec(
    name="SKM_Player_Male01", base="SKM_Manny_Simple",
    head=HeadDims(),
    hair=SHORT,
    outfit=Outfit(sleeve=0.50, neck_dip=0.012, hem=-0.035, skirt=False, shorts=0.52, sock_top=0.20, headband=False),
    skin=m.PLAYER_SKIN_A, hair_color=m.PLAYER_HAIR_A, top=m.PLAYER_TOP_A, accent=m.PLAYER_BAND_A,
)

FEMALE01 = PlayerSpec(
    name="SKM_Player_Female01", base="SKM_Quinn_Simple",
    head=HeadDims(width=0.95, depth=0.95, height=0.94, neck=0.86),
    hair=TIED,
    outfit=Outfit(sleeve=None, neck_dip=0.045, hem=0.020, skirt=True, shorts=0.28, sock_top=0.13, headband=True),
    skin=m.PLAYER_SKIN_B, hair_color=m.PLAYER_HAIR_B, top=m.PLAYER_TOP_B, accent=m.PLAYER_BAND_B,
)

PLAYERS: tuple[PlayerSpec, ...] = (MALE01, FEMALE01)

# 服の厚み（体の面からの距離、m）。重なるところ（腰）はシャツを外にする
TOP_OFFSET = 0.011
SHORTS_OFFSET = 0.008
SHORTS_FLARE = 0.012       # ショートパンツの裾の広がり（m）
SHOE_OFFSET = 0.009
SHOE_TOP = 0.095           # 靴の上端（m）
SOLE_TOP = 0.022           # 靴底の上端（m）
WRIST_BAND = (0.86, 1.04)  # リストバンドの範囲（前腕の割合：lowerarm → hand）。作り直した前腕と元の手のつなぎ目を覆うので、2 人とも付ける
WRIST_OFFSET = 0.006
SKIRT_LENGTH = 0.42        # スカートの裾（太ももの割合）
SKIRT_FLARE = 0.070        # 裾の広がり（m）
SKIRT_PLEATS = 16
