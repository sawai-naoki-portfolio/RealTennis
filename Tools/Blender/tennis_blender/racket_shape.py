"""ラケットの形（純粋な関数。bpy に依存しない）。

局所座標（dims の説明）：原点＝グリップエンド、+X＝グリップ → 先端、Y＝フェイスの幅、Z＝フェイスの法線。
形は XY・XZ の両方の面について対称にする（FBX で Y が反転しても形が変わらない。ART-03 §3）。

- ヘッド：打球面の楕円の縁から外向きの法線の方へ、八角形の断面を一周させる（内側の縁＝打球面の楕円）
- スロート：シャフトの先からヘッドの下側の 2 点へ、まっすぐな腕（断面はヘッドより少し細い）
- グリップ・シャフト：X 軸に沿って八角形の断面をつなぐ（バットキャップ → グリップ → シャフト）
- ストリング：縦糸・横糸を細い角柱で表す（見た目用）。端はフレームの中に埋める
"""
from __future__ import annotations

import math

from . import dims as d
from . import materials as mat
from .primitives import Mesh, Vec3, box, join, octagon, regular_octagon, straight_tube, tube

FACE_A = d.RACKET_FACE_LENGTH / 2                          # 打球面の楕円の半径（X）
FACE_B = d.RACKET_FACE_WIDTH / 2                           # 同（Y）
FACE_CENTER_X = d.RACKET_LENGTH - d.RACKET_FRAME_WIDTH - FACE_A
SOCKET_SWEETSPOT = "SOCKET_racket_sweetspot"               # UE のソケット racket_sweetspot（DD-03 §7）
SOCKET_GRIP = "SOCKET_grip"                                # UE のソケット grip


def head() -> Mesh:
    """ヘッドのフレーム。外側の縁は打球面の楕円の平行曲線で、軸の端で (A + 幅, B + 幅) になる。"""
    n = d.RACKET_RING_SEGMENTS
    if n % 4:
        raise ValueError(f"RACKET_RING_SEGMENTS は 4 の倍数: {n}")
    section = octagon(d.RACKET_FRAME_WIDTH / 2, d.RACKET_BEAM / 2, d.RACKET_FRAME_CHAMFER)
    angles = [-2.0 * math.pi * i / n for i in range(n)]  # 時計回り（横＝外向きの法線のとき、横 × Z が進む向きになる）
    return tube([(_frame_center(th), _normal(th), section) for th in angles], closed=True)


def throat() -> Mesh:
    """シャフトの先からヘッドの中心線への 2 本の腕。根元はシャフトに、先はフレームに埋まる。"""
    inset = d.RACKET_OVERLAP_INSET
    section = octagon(d.RACKET_FRAME_WIDTH / 2 - inset, d.RACKET_BEAM / 2 - inset, d.RACKET_FRAME_CHAMFER)
    start = (d.RACKET_THROAT_X - d.RACKET_FRAME_WIDTH, 0.0, 0.0)
    ends = [_frame_center(math.pi - side * math.radians(d.RACKET_THROAT_JOIN_DEG)) for side in (1.0, -1.0)]
    return join([straight_tube(start, end, section) for end in ends])


def grip() -> Mesh:
    """バットキャップとグリップ（端面はふさぐ。シャフト側はシャフトの断面とつながる）。"""
    butt, wrap = regular_octagon(d.RACKET_BUTT_ACROSS), regular_octagon(d.RACKET_GRIP_ACROSS)
    xs = [(0.0, butt), (d.RACKET_BUTT_LENGTH, butt), (d.RACKET_BUTT_LENGTH, wrap), (d.RACKET_GRIP_LENGTH, wrap)]
    verts, faces = tube([((x, 0.0, 0.0), (0.0, 1.0), s) for x, s in xs])
    return verts, faces + [tuple(reversed(range(8)))]


def shaft() -> Mesh:
    """グリップの端からスロートまで細くなるシャフト（先はふさぐ。腕が突き抜ける）。"""
    tip = octagon(d.RACKET_SHAFT_WIDTH / 2, d.RACKET_BEAM / 2, d.RACKET_FRAME_CHAMFER)
    xs = [(d.RACKET_GRIP_LENGTH, regular_octagon(d.RACKET_GRIP_ACROSS)), (d.RACKET_THROAT_X, tip)]
    verts, faces = tube([((x, 0.0, 0.0), (0.0, 1.0), s) for x, s in xs])
    return verts, faces + [tuple(range(8, 16))]


def main_ys() -> list[float]:
    """縦糸の Y（打球面の幅を本数 + 1 に等分した点）。"""
    n = d.RACKET_STRING_MAINS
    return [-FACE_B + d.RACKET_FACE_WIDTH * (i + 1) / (n + 1) for i in range(n)]


def cross_xs() -> list[float]:
    """横糸の X（打球面の長さを本数 + 1 に等分した点）。"""
    n = d.RACKET_STRING_CROSSES
    return [FACE_CENTER_X - FACE_A + d.RACKET_FACE_LENGTH * (i + 1) / (n + 1) for i in range(n)]


def strings() -> Mesh:
    """縦糸（X 方向）と横糸（Y 方向）の角柱。端は打球面の縁からフレームの幅の半分だけ延ばす（フレームに埋める）。

    横糸は縦糸より薄くして、交わる所で上下の面が同じ平面に乗らないようにする。
    """
    g = d.RACKET_STRING_GAUGE / 2
    gc = g - d.RACKET_OVERLAP_INSET
    embed = d.RACKET_FRAME_WIDTH / 2
    mains = [(FACE_A * math.sqrt(1.0 - (y / FACE_B) ** 2) + embed, y) for y in main_ys()]
    crosses = [(FACE_B * math.sqrt(1.0 - ((x - FACE_CENTER_X) / FACE_A) ** 2) + embed, x) for x in cross_xs()]
    return join([box((FACE_CENTER_X - h, FACE_CENTER_X + h), (y - g, y + g), (-g, g)) for h, y in mains]
                + [box((x - g, x + g), (-h, h), (-gc, gc)) for h, x in crosses])


def parts() -> list[tuple[str, Mesh]]:
    """部品と、そのマテリアルのスロット。"""
    return [(mat.RACKET_GRIP, grip()), (mat.RACKET_FRAME, shaft()), (mat.RACKET_FRAME, throat()),
            (mat.RACKET_FRAME, head()), (mat.RACKET_STRINGS, strings())]


def sockets() -> dict[str, Vec3]:
    """ソケットの位置（回転はメッシュの軸のまま）。"""
    return {SOCKET_SWEETSPOT: (FACE_CENTER_X - d.RACKET_SWEETSPOT_OFFSET, 0.0, 0.0),
            SOCKET_GRIP: (d.RACKET_GRIP_SOCKET_X, 0.0, 0.0)}


def _normal(th: float) -> tuple[float, float]:
    """打球面の楕円の、角度 th（+X から）の点での外向きの単位法線。"""
    nx, ny = FACE_B * math.cos(th), FACE_A * math.sin(th)
    length = math.hypot(nx, ny)
    return nx / length, ny / length


def _frame_center(th: float) -> Vec3:
    """ヘッドの断面の中心線の点（打球面の楕円の縁から、外向きの法線の方へフレームの幅の半分）。"""
    (nx, ny), half = _normal(th), d.RACKET_FRAME_WIDTH / 2
    return (FACE_CENTER_X + FACE_A * math.cos(th) + nx * half, FACE_B * math.sin(th) + ny * half, 0.0)
