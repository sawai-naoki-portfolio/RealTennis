"""仮の選手の頭の部品（純粋な関数。bpy に依存しない）：耳・目・眉・髪・ポニーテール・ヘッドバンド。

座標と単位は head_shape の頭の座標（m。x＝選手の左、y＝後ろ、z＝上、原点はあご先の高さの耳の軸）。面は外を向く。
"""
from __future__ import annotations

import math
from dataclasses import dataclass

from .head_shape import (CROWN_Z, EYE_X, EYE_Z, EYEBALL_R, IRIS_R, HeadDims, Mesh, Vec3, pchip, surface_point,
                         theta_for_x)

SKULL_CENTER_Z = 0.11  # 髪の厚みを付ける向きの中心（頭の座標の z。倍率の前）


@dataclass(frozen=True)
class HairStyle:
    hairline: tuple[float, float, float, float, float]  # 生え際の z：前・こめかみ（45°）・横（90°）・耳の後ろ（135°）・後ろ（180°）
    edge: float  # 生え際の厚み（m）
    body: float  # 厚み
    top: float   # 頭頂の厚み
    ponytail: bool = False


SHORT = HairStyle((0.190, 0.172, 0.150, 0.080, 0.062), 0.002, 0.009, 0.010)
TIED = HairStyle((0.192, 0.170, 0.142, 0.085, 0.040), 0.003, 0.010, 0.011, ponytail=True)


def _norm(v: Vec3) -> Vec3:
    n = math.sqrt(sum(c * c for c in v)) or 1.0
    return v[0] / n, v[1] / n, v[2] / n


def _add(a: Vec3, b: Vec3, s: float = 1.0) -> Vec3:
    return a[0] + b[0] * s, a[1] + b[1] * s, a[2] + b[2] * s


def _cross(a: Vec3, b: Vec3) -> Vec3:
    return a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]


def hairline_z(style: HairStyle, theta: float) -> float:
    """生え際の高さ（倍率の前）。θ は 0＝前、左右は対称。前と後ろで傾きが 0 になるように、キーを鏡で足す。"""
    a = abs(math.remainder(theta, 2 * math.pi))
    front, temple, side, behind, back = style.hairline
    q = math.pi / 4
    return pchip([-q, 0.0, q, 2 * q, 3 * q, 4 * q, 5 * q], [temple, front, temple, side, behind, back, behind], a)


def hair_thickness(style: HairStyle, s: float) -> float:
    """生え際（s＝0）から頭頂（s＝1）までの厚み。生え際の 15% で edge から body へ上げ、頭頂へ向けて top へ。"""
    t = min(1.0, s / 0.15)
    return style.edge + (style.body - style.edge) * t * t * (3 - 2 * t) + (style.top - style.body) * s * s


def _hair_point(style: HairStyle, dims: HeadDims, theta: float, s: float) -> Vec3:
    z0 = hairline_z(style, theta) * dims.height
    z = z0 + (CROWN_Z * dims.height * 0.995 - z0) * s
    p = surface_point(theta, z, dims, features=False)
    d = _norm((p[0], p[1], p[2] - SKULL_CENTER_Z * dims.height))
    return _add(p, d, hair_thickness(style, s))


def hair_mesh(style: HairStyle, dims: HeadDims = HeadDims(), segments: int = 64, rows: int = 18) -> Mesh:
    """髪の殻（下の縁は開いている）。列は θ の順、行は生え際から頭頂へ。最後の頂点は頭頂。"""
    verts = [_hair_point(style, dims, 2 * math.pi * j / segments, k / rows) for k in range(rows) for j in range(segments)]
    top = (rows - 1) * segments
    verts.append((0.0, sum(v[1] for v in verts[top:]) / segments, CROWN_Z * dims.height + style.top))
    faces: list[tuple[int, ...]] = []
    for k in range(rows - 1):
        a, b = k * segments, (k + 1) * segments
        faces += [(a + j, a + (j + 1) % segments, b + (j + 1) % segments, b + j) for j in range(segments)]
    faces += [(top + j, top + (j + 1) % segments, len(verts) - 1) for j in range(segments)]
    return verts, faces


def _ring(center: Vec3, u: Vec3, v: Vec3, ru: float, rv: float, sides: int) -> list[Vec3]:
    """中心のまわりの楕円（u から v の向きへ回る）。"""
    return [_add(_add(center, u, ru * math.cos(2 * math.pi * i / sides)), v, rv * math.sin(2 * math.pi * i / sides))
            for i in range(sides)]


def loft(rings: list[list[Vec3]], tip: Vec3 | None = None) -> Mesh:
    """輪を順につなぐ（輪は進む向きのまわりに反時計回り）。tip があれば最後の輪を 1 点に閉じる。"""
    sides = len(rings[0])
    verts = [p for r in rings for p in r]
    faces = [(i * sides + j, i * sides + (j + 1) % sides, (i + 1) * sides + (j + 1) % sides, (i + 1) * sides + j)
             for i in range(len(rings) - 1) for j in range(sides)]
    if tip is not None:
        last = (len(rings) - 1) * sides
        verts.append(tip)
        faces += [(last + j, last + (j + 1) % sides, len(verts) - 1) for j in range(sides)]
    return verts, faces


def ear_mesh(side: int, dims: HeadDims = HeadDims(), sides: int = 12) -> Mesh:
    """耳（side：+1＝左、−1＝右）。頭の横から少し後ろに、外と少し前を向けて付ける。根元は頭に埋める。"""
    zc = 0.114 * dims.height
    p = surface_point(side * (math.pi / 2 + 0.10), zc, dims, features=False)
    n = _norm((float(side), -0.25, 0.0))
    u = _norm((0.0, 0.26, 1.0))
    v = _cross(n, u)
    stations = ((1.00, -0.004), (1.00, 0.004), (0.95, 0.009), (0.70, 0.010), (0.55, 0.005))
    rings = [_ring(_add(p, n, off), u, v, 0.031 * s, 0.018 * s, sides) for s, off in stations]
    return loft(rings, tip=_add(p, n, 0.003))


def uv_sphere(center: Vec3, radius: float, scale: Vec3 = (1.0, 1.0, 1.0), segments: int = 12, rings: int = 8) -> Mesh:
    verts = [(center[0], center[1], center[2] - radius * scale[2])]
    for i in range(1, rings):
        phi = -math.pi / 2 + math.pi * i / rings
        for j in range(segments):
            th = 2 * math.pi * j / segments
            verts.append((center[0] + radius * scale[0] * math.cos(phi) * math.cos(th),
                          center[1] + radius * scale[1] * math.cos(phi) * math.sin(th),
                          center[2] + radius * scale[2] * math.sin(phi)))
    verts.append((center[0], center[1], center[2] + radius * scale[2]))
    top = len(verts) - 1
    faces: list[tuple[int, ...]] = [(0, 1 + (j + 1) % segments, 1 + j) for j in range(segments)]
    for i in range(rings - 2):
        a, b = 1 + i * segments, 1 + (i + 1) * segments
        faces += [(a + j, a + (j + 1) % segments, b + (j + 1) % segments, b + j) for j in range(segments)]
    last = 1 + (rings - 2) * segments
    faces += [(last + j, last + (j + 1) % segments, top) for j in range(segments)]
    return verts, faces


def eye_meshes(side: int, dims: HeadDims = HeadDims()) -> tuple[Mesh, Mesh]:
    """(白目, 黒目)。白目は目のくぼみから 4.5 mm 出る。黒目は白目の前の面に平たく重ねる。"""
    x, z = side * EYE_X * dims.width, EYE_Z * dims.height
    s = surface_point(theta_for_x(x, z, dims), z, dims)
    eye = (x, s[1] + EYEBALL_R - 0.0045, z)
    iris = (x, eye[1] - EYEBALL_R + 0.0012, z)
    return uv_sphere(eye, EYEBALL_R), uv_sphere(iris, IRIS_R, scale=(1.0, 0.35, 1.0))


def brow_mesh(side: int, dims: HeadDims = HeadDims(), samples: int = 8) -> Mesh:
    """眉（細い帯。表面から 2.5 mm 出す）。内（鼻の側）から外へ、少し弓なりに細くなる。"""
    front, back = [], []
    for i in range(samples):
        t = i / (samples - 1)
        x = (0.013 + 0.037 * t) * dims.width
        zc = (0.141 + 0.005 * math.sin(math.pi * min(1.0, t * 1.25))) * dims.height
        half = 0.0035 - 0.0018 * t
        for z, row in ((zc - half, 0), (zc + half, 1)):
            p = surface_point(theta_for_x(x, z, dims), z, dims)
            back.append(p)
            front.append((p[0], p[1] - 0.0025, p[2]))
    verts = [(side * p[0], p[1], p[2]) for p in front + back]
    n = 2 * samples
    faces: list[tuple[int, ...]] = []
    for i in range(samples - 1):
        b0, t0, b1, t1 = 2 * i, 2 * i + 1, 2 * i + 2, 2 * i + 3
        faces.append((b0, b1, t1, t0))                  # 前
        faces.append((t0, t1, t1 + n, t0 + n))          # 上
        faces.append((b1, b0, b0 + n, b1 + n))          # 下
    faces.append((0, 1, 1 + n, n))                      # 内の端
    last = 2 * (samples - 1)
    faces.append((last + 1, last, last + n, last + 1 + n))  # 外の端
    if side < 0:  # x を反転すると向きが逆になる
        faces = [tuple(reversed(f)) for f in faces]
    return verts, faces


def _frame_tube(path: list[Vec3], radii: list[float], sides: int, tip: Vec3 | None) -> Mesh:
    """YZ の面の中の道に沿った筒（断面は円。x 軸を断面の 1 つの向きにする）。"""
    rings = []
    for i, (p, r) in enumerate(zip(path, radii)):
        q0, q1 = path[max(0, i - 1)], path[min(len(path) - 1, i + 1)]
        t = _norm((q1[0] - q0[0], q1[1] - q0[1], q1[2] - q0[2]))
        a = (1.0, 0.0, 0.0)
        b = _cross(t, a)
        rings.append(_ring(p, a, b, r, r, sides))
    return loft(rings, tip)


def ponytail_mesh(style: HairStyle, dims: HeadDims = HeadDims(), stations: int = 10, sides: int = 10) -> tuple[Mesh, Mesh]:
    """(ポニーテール, 結び目のゴム)。後頭部の上から、後ろへ少し出て下へ垂れる。"""
    base = surface_point(math.pi, 0.150 * dims.height, dims, features=False)
    path, radii = [], []
    for i in range(stations):
        t = i / (stations - 1)
        path.append((0.0, base[1] + style.body + 0.040 * math.sin(t * math.pi * 0.6), base[2] - 0.20 * t))
        radii.append(0.023 * (1.0 + 0.35 * math.sin(math.pi * min(1.0, t * 1.4))) * (1.0 - 0.65 * t))
    tail = _frame_tube(path, radii, sides, tip=(0.0, path[-1][1] - 0.004, path[-1][2] - 0.012))
    tie_path = [path[0], _add(path[0], _norm(_add(path[1], path[0], -1.0)), 0.012)]
    tie = _frame_tube(tie_path, [radii[0] + 0.003, radii[0] + 0.003], sides, tip=None)
    return tail, tie


def headband_mesh(style: HairStyle, dims: HeadDims = HeadDims(), segments: int = 48) -> Mesh:
    """ヘッドバンド（幅 2.4 cm・厚み 3 mm の輪）。前は額の上、後ろは少し下げる。髪の上に置く。"""
    rings = []
    for z_off in (-0.012, 0.012):
        for depth in (0.0005, 0.0035):
            ring = []
            for j in range(segments):
                th = 2 * math.pi * j / segments
                z = (0.163 + 0.011 * math.cos(th) + z_off) * dims.height
                z0 = hairline_z(style, th) * dims.height
                s = max(0.0, (z - z0) / (CROWN_Z * dims.height * 0.995 - z0))
                p = surface_point(th, z, dims, features=False)
                d = _norm((p[0], p[1], 0.0))
                ring.append(_add(p, d, hair_thickness(style, s) + depth))
            rings.append(ring)
    inner_lo, outer_lo, inner_hi, outer_hi = rings
    verts = inner_lo + outer_lo + outer_hi + inner_hi
    faces: list[tuple[int, ...]] = []
    for k in range(4):  # 下 → 外 → 上 → 内 の 4 面
        a, b = k * segments, ((k + 1) % 4) * segments
        faces += [(a + j, a + (j + 1) % segments, b + (j + 1) % segments, b + j) for j in range(segments)]
    return verts, faces
