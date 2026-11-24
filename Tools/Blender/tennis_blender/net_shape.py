"""ネットの形（純粋な関数。bpy に依存しない）。

- 上辺の高さ：ポスト → シングルススティックは 1.07 m 一定、スティック → 中央は 0.914 m まで直線（RES-01 §1）
- 本体：X = 0 の平面の格子。列ごとに、下辺 z = 0 から上辺まで等分する
- 白帯：上辺をコードの太さで包む U 字の帯（本体と同じ平面に重ならない）
- 揺れの重み（頂点カラー R）：(1 - |y|/y_post) × (z/上辺の高さ)。ポスト際と下辺で 0、上辺の中央で 1
"""
from __future__ import annotations

from . import dims as d

Vert = tuple[float, float, float]
Face = tuple[int, int, int, int]


def top_height(y: float) -> float:
    """y（m）での上辺（ネットコードの上）の高さ。"""
    a = abs(y)
    if a >= d.STICK_Y:
        return d.NET_HEIGHT_POST
    return d.NET_HEIGHT_CENTER + (d.NET_HEIGHT_POST - d.NET_HEIGHT_CENTER) * a / d.STICK_Y


def column_ys(segments: int) -> list[float]:
    """横の分割の y（-y_post → +y_post の segments + 1 個）。

    上辺が折れる y = ±STICK_Y と中央 y = 0 に必ず頂点を置き、区間ごとにほぼ等間隔に分ける。
    """
    if segments < 4 or segments % 2:
        raise ValueError(f"segments は 4 以上の偶数: {segments}")
    half = segments // 2
    outer = max(1, round(half * (d.POST_Y - d.STICK_Y) / d.POST_Y))
    inner = half - outer
    right = [d.STICK_Y * i / inner for i in range(inner + 1)]
    right += [d.STICK_Y + (d.POST_Y - d.STICK_Y) * i / outer for i in range(1, outer + 1)]
    return [-y for y in reversed(right[1:])] + right


def sway_weight(y: float, z: float) -> float:
    """揺れの重み（0〜1）。UE の WPO で、衝突の揺れ幅に掛ける。"""
    w = (1.0 - abs(y) / d.POST_Y) * (z / top_height(y))
    return min(1.0, max(0.0, w))


def net_grid(segments_y: int, segments_z: int) -> tuple[list[Vert], list[Face]]:
    """ネット本体の頂点と面（法線 +X）。頂点は列（y）ごとに下から上へ並ぶ。"""
    if segments_z < 1:
        raise ValueError(f"segments_z は 1 以上: {segments_z}")
    n = segments_z + 1
    verts = [(0.0, y, top_height(y) * j / segments_z) for y in column_ys(segments_y) for j in range(n)]
    faces = [(i * n + j, (i + 1) * n + j, (i + 1) * n + j + 1, i * n + j + 1)
             for i in range(segments_y) for j in range(segments_z)]
    return verts, faces


def band(segments_y: int) -> tuple[list[Vert], list[Face]]:
    """白帯の頂点と面。断面は上辺を包む U 字（+X 側の面・上面・-X 側の面）で、面は外向き。"""
    c = d.CORD_DIAMETER / 2
    verts: list[Vert] = []
    for y in column_ys(segments_y):
        top = top_height(y)
        bottom = top - d.BAND_DEPTH
        verts += [(c, y, bottom), (c, y, top), (-c, y, top), (-c, y, bottom)]
    faces: list[Face] = []
    for i in range(segments_y):
        a, b = 4 * i, 4 * (i + 1)
        faces.append((a + 0, b + 0, b + 1, a + 1))  # +X 側
        faces.append((a + 1, b + 1, b + 2, a + 2))  # 上面
        faces.append((a + 2, b + 2, b + 3, a + 3))  # -X 側
    return verts, faces
