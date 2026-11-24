"""形の部品（純粋な関数。bpy に依存しない）：断面の多角形、断面をつなぐ筒、縦の柱、軸にそろった箱、結合。

頂点は m。面は外を向く（外から見て反時計回り）。
"""
from __future__ import annotations

import math

Vec3 = tuple[float, float, float]
Mesh = tuple[list[Vec3], list[tuple[int, ...]]]
Section = list[tuple[float, float]]
Station = tuple[Vec3, tuple[float, float], Section]  # (中心, 横の向き（XY の単位ベクトル）, 断面)


def octagon(half_w: float, half_t: float, chamfer: float) -> Section:
    """長方形の角を chamfer だけ落とした八角形（横 u・縦 v）。(u, v) の平面で反時計回り。"""
    if not 0.0 < chamfer < min(half_w, half_t):
        raise ValueError(f"chamfer は 0 より大きく、半分の幅と厚さより小さい: {chamfer}")
    w, t, c = half_w, half_t, chamfer
    return [(w, -t + c), (w, t - c), (w - c, t), (-w + c, t),
            (-w, t - c), (-w, -t + c), (-w + c, -t), (w - c, -t)]


def regular_octagon(across: float) -> Section:
    """対辺の距離が across の正八角形（辺が軸に平行。バウンディングボックスの幅が across になる）。"""
    return octagon(across / 2, across / 2, across / (2 + math.sqrt(2)))


def regular_polygon(diameter: float, sides: int) -> Section:
    """外接円の直径が diameter の正多角形（反時計回り）。sides が 4 の倍数なら頂点が軸の上に乗り、幅が diameter になる。"""
    if sides < 3 or diameter <= 0.0:
        raise ValueError(f"sides は 3 以上、diameter は正: {sides}, {diameter}")
    r = diameter / 2
    return [(r * math.cos(2 * math.pi * i / sides), r * math.sin(2 * math.pi * i / sides)) for i in range(sides)]


def tube(stations: list[Station], closed: bool = False, caps: bool = False) -> Mesh:
    """断面を順につないだ筒。断面の点 (u, v) は 中心 + 横 × u + Z × v に置く（進む向きは XY の平面の中）。

    横の向きは、進む向き D に対して (-D.y, D.x)（横 × Z ＝ D）にする。このとき面は外を向く。
    closed は最後と最初をつなぐ（輪）。caps は両端を n 角形でふさぐ（closed のときは使わない）。
    """
    if len(stations) < 2 or (closed and caps):
        raise ValueError("断面は 2 つ以上。closed と caps は同時に使わない")
    k = len(stations[0][2])
    if k < 3 or any(len(s) != k for _, _, s in stations):
        raise ValueError("断面の点の数は 3 以上で、すべて同じ")
    verts: list[Vec3] = []
    for (cx, cy, cz), (lx, ly), section in stations:
        verts += [(cx + lx * u, cy + ly * u, cz + v) for u, v in section]
    n = len(stations)
    faces: list[tuple[int, ...]] = []
    for i in range(n if closed else n - 1):
        a, b = i * k, ((i + 1) % n) * k
        faces += [(a + j, a + (j + 1) % k, b + (j + 1) % k, b + j) for j in range(k)]
    if caps:
        faces.append(tuple(reversed(range(k))))
        faces.append(tuple(range((n - 1) * k, n * k)))
    return verts, faces


def straight_tube(start: Vec3, end: Vec3, section: Section, caps: bool = True) -> Mesh:
    """start から end（XY の平面の中の向き）へのまっすぐな筒。"""
    dx, dy = end[0] - start[0], end[1] - start[1]
    length = math.hypot(dx, dy)
    if length <= 0.0:
        raise ValueError(f"start と end が同じ（向きが XY の平面にない）: {start}")
    lateral = (-dy / length, dx / length)
    return tube([(start, lateral, section), (end, lateral, section)], caps=caps)


def prism(center: tuple[float, float], section: Section, z0: float, z1: float) -> Mesh:
    """XY の平面の多角形 section（反時計回り）を、center に z0 → z1 まで立てた柱（上下をふさぐ）。"""
    if z1 <= z0 or len(section) < 3:
        raise ValueError(f"z1 > z0、断面の点は 3 以上: {z0}, {z1}, {len(section)}")
    cx, cy = center
    k = len(section)
    verts = [(cx + u, cy + v, z) for z in (z0, z1) for u, v in section]
    faces: list[tuple[int, ...]] = [(j, (j + 1) % k, k + (j + 1) % k, k + j) for j in range(k)]
    faces += [tuple(reversed(range(k))), tuple(range(k, 2 * k))]
    return verts, faces


def box(xs: tuple[float, float], ys: tuple[float, float], zs: tuple[float, float]) -> Mesh:
    """軸にそろった箱（xs・ys・zs は (最小, 最大)）。頂点の番号は 4 × ix + 2 × iy + iz。"""
    verts = [(x, y, z) for x in xs for y in ys for z in zs]
    faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    return verts, faces


def join(meshes: list[Mesh]) -> Mesh:
    """メッシュを 1 つにまとめる（頂点の番号をずらす）。"""
    verts: list[Vec3] = []
    faces: list[tuple[int, ...]] = []
    for v, f in meshes:
        faces += [tuple(len(verts) + i for i in face) for face in f]
        verts += v
    return verts, faces


def merge_slots(items: list[tuple[str, Mesh]]) -> tuple[list[Vec3], list[tuple[int, ...]], list[str], list[int]]:
    """スロットつきの部品を 1 つのメッシュにまとめる：(頂点, 面, スロットの名前（出てきた順）, 面ごとのスロットの番号)。"""
    names = list(dict.fromkeys(name for name, _ in items))
    verts, faces = join([mesh for _, mesh in items])
    indices = [names.index(name) for name, (_, f) in items for _ in f]
    return verts, faces, names, indices
