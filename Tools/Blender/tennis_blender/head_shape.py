"""仮の選手（ART-01 A-CHR-004・005）の頭と首の形（純粋な関数。bpy に依存しない）。

UE の Manny・Quinn の頭はヘルメットの形で顔がないので、頭と首は作り直す。座標は「頭の座標」（m）：
x＝選手の左（Blender の +X）、y＝後ろ（Blender の +Y。選手は −Y を向く）、z＝上。原点は、あご先の高さで耳の軸の上。
断面（横の半分の幅 W・耳の軸から前 F・後ろ B）を高さで補間し、顔（目のくぼみ・鼻・唇・あご・頬）は前の向きの変位で足す。
基準は成人の男性の頭（あご → 頭頂 0.230 m、幅 0.156 m）で、HeadDims の倍率で変える。面は外を向く（外から見て反時計回り）。
"""
from __future__ import annotations

import math
from dataclasses import dataclass

Vec3 = tuple[float, float, float]
Mesh = tuple[list[Vec3], list[tuple[int, ...]]]


@dataclass(frozen=True)
class HeadDims:
    width: float = 1.0   # 横の倍率
    depth: float = 1.0   # 前後の倍率
    height: float = 1.0  # 上下の倍率
    neck: float = 1.0    # 首の太さの倍率


# 断面のキー：(z, W, F, B, 前の指数, 後ろの指数)。z はあご先から。指数が小さいほど前（あご）がとがる
PROFILE: tuple[tuple[float, float, float, float, float, float], ...] = (
    (-0.115, 0.105, 0.075, 0.135, 2.0, 2.0),  # 首の付け根（シャツの中。襟の高さで襟ぐりより太くして、体の穴をふさぐ）
    (-0.085, 0.099, 0.069, 0.129, 2.0, 2.0),
    (-0.065, 0.090, 0.060, 0.120, 2.0, 2.0),
    (-0.045, 0.072, 0.042, 0.102, 2.0, 2.0),
    (-0.030, 0.063, 0.037, 0.092, 2.0, 2.0),
    (-0.018, 0.060, 0.042, 0.083, 2.0, 2.0),  # のど
    (0.000, 0.056, 0.078, 0.082, 1.7, 2.0),   # あごの下
    (0.012, 0.054, 0.090, 0.082, 1.7, 2.0),   # あご先
    (0.035, 0.060, 0.094, 0.084, 1.8, 2.1),
    (0.055, 0.064, 0.097, 0.088, 2.0, 2.1),   # 口
    (0.078, 0.068, 0.099, 0.093, 2.2, 2.2),   # 鼻の下
    (0.100, 0.072, 0.099, 0.099, 2.3, 2.2),   # 頬骨
    (0.118, 0.075, 0.097, 0.103, 2.4, 2.2),   # 目
    (0.138, 0.077, 0.101, 0.106, 2.5, 2.2),   # 眉
    (0.160, 0.078, 0.099, 0.107, 2.5, 2.2),
    (0.182, 0.077, 0.092, 0.104, 2.4, 2.2),
    (0.200, 0.072, 0.080, 0.096, 2.3, 2.2),
    (0.215, 0.062, 0.062, 0.082, 2.2, 2.2),
    (0.225, 0.045, 0.040, 0.060, 2.0, 2.0),
)
CROWN_Z = 0.230
NECK_BOTTOM_Z = PROFILE[0][0]
EYE_X, EYE_Z = 0.031, 0.118
EYEBALL_R, IRIS_R = 0.0118, 0.0058

# 顔の変位（ガウスのふくらみ）：(x0, z0, σx, σz, 高さ, 左右に鏡で置くか)
FEATURES: tuple[tuple[float, float, float, float, float, bool], ...] = (
    (EYE_X, EYE_Z, 0.012, 0.010, -0.007, True),  # 目のくぼみ
    (0.026, 0.140, 0.017, 0.006, 0.003, True),    # 眉の骨
    (0.050, 0.100, 0.015, 0.013, 0.005, True),    # 頬骨
    (0.014, 0.077, 0.006, 0.006, 0.006, True),    # 小鼻
    (0.000, 0.061, 0.016, 0.006, 0.005, False),   # 上唇
    (0.000, 0.052, 0.020, 0.0018, -0.003, False),  # 口の線
    (0.000, 0.043, 0.015, 0.005, 0.006, False),   # 下唇
    (0.000, 0.014, 0.014, 0.010, 0.006, False),   # あご
)
# 鼻筋：(z, 高さ, σx)。z の間は直線で補間し、外は 0
NOSE: tuple[tuple[float, float, float], ...] = (
    (0.066, 0.000, 0.012), (0.072, 0.010, 0.012), (0.080, 0.021, 0.011), (0.088, 0.020, 0.010),
    (0.105, 0.010, 0.008), (0.125, 0.004, 0.007), (0.135, 0.000, 0.008),
)
LIPS = (0.024, 0.038, 0.066)  # 唇のマテリアルの範囲：|x| < 0.024、0.038 < z < 0.066（顔の前）


def pchip(xs: list[float], ys: list[float], x: float) -> float:
    """単調な 3 次エルミート補間（Fritsch–Carlson）。キーの間で行き過ぎない。範囲の外は端の値。"""
    n = len(xs)
    if x <= xs[0]:
        return ys[0]
    if x >= xs[-1]:
        return ys[-1]
    h = [xs[i + 1] - xs[i] for i in range(n - 1)]
    d = [(ys[i + 1] - ys[i]) / h[i] for i in range(n - 1)]
    m = [d[0]] + [0.0 if d[i - 1] * d[i] <= 0 else
                  (3 * (h[i - 1] + h[i])) / ((2 * h[i] + h[i - 1]) / d[i - 1] + (h[i] + 2 * h[i - 1]) / d[i])
                  for i in range(1, n - 1)] + [d[-1]]
    i = max(k for k in range(n - 1) if xs[k] <= x)
    t = (x - xs[i]) / h[i]
    h00, h10, h01, h11 = 2 * t**3 - 3 * t**2 + 1, t**3 - 2 * t**2 + t, -2 * t**3 + 3 * t**2, t**3 - t**2
    return h00 * ys[i] + h10 * h[i] * m[i] + h01 * ys[i + 1] + h11 * h[i] * m[i + 1]


def profile_at(z: float, dims: HeadDims = HeadDims()) -> tuple[float, float, float, float, float]:
    """高さ z（倍率を掛けた後の m）の断面 (W, F, B, 前の指数, 後ろの指数)。頭頂より上は 0。"""
    zr = z / dims.height
    if zr >= CROWN_Z:
        return 0.0, 0.0, 0.0, 2.0, 2.0
    keys = list(PROFILE) + [(CROWN_Z, 0.0, 0.0, 0.0, 2.0, 2.0)]
    zs = [k[0] for k in keys]
    w, f, b, nf, nb = (pchip(zs, [k[i] for k in keys], zr) for i in range(1, 6))
    if zr < 0.0:  # 首：太さだけ変える（中心はそのまま）
        center, half = (f - b) / 2, (f + b) / 2 * dims.neck
        w, f, b = w * dims.neck, center + half, half - center
    return w * dims.width, f * dims.depth, b * dims.depth, nf, nb


def _spow(v: float, p: float) -> float:
    return math.copysign(abs(v) ** p, v)


def nose_height(x: float, z: float) -> float:
    zs = [k[0] for k in NOSE]
    if not zs[0] < z < zs[-1]:
        return 0.0
    i = max(k for k in range(len(NOSE) - 1) if zs[k] <= z)
    t = (z - zs[i]) / (zs[i + 1] - zs[i])
    amp = NOSE[i][1] + (NOSE[i + 1][1] - NOSE[i][1]) * t
    sx = NOSE[i][2] + (NOSE[i + 1][2] - NOSE[i][2]) * t
    return amp * math.exp(-x * x / (2 * sx * sx))


def face_offset(x: float, z: float) -> float:
    """顔の変位（m。正は外へ）。x・z は基準の頭の大きさの座標。"""
    total = nose_height(x, z)
    for x0, z0, sx, sz, amp, mirrored in FEATURES:
        for xc in ((x0, -x0) if mirrored else (x0,)):
            total += amp * math.exp(-((x - xc) ** 2) / (2 * sx * sx) - ((z - z0) ** 2) / (2 * sz * sz))
    return total


def _front_weight(c: float) -> float:
    """顔の変位を前だけに効かせる重み（cos θ が 0.25 以下で 0、0.65 以上で 1）。"""
    t = min(1.0, max(0.0, (c - 0.25) / 0.40))
    return t * t * (3 - 2 * t)


def surface_point(theta: float, z: float, dims: HeadDims = HeadDims(), features: bool = True) -> Vec3:
    """頭の表面の点（頭の座標）。θ＝0 が前、増えると選手の左（+x）へ回る。"""
    w, f, b, nf, nb = profile_at(z, dims)
    s, c = math.sin(theta), math.cos(theta)
    n = nf if c >= 0 else nb
    center, half = (f - b) / 2, (f + b) / 2
    x, fwd = w * _spow(s, 2 / n), center + half * _spow(c, 2 / n)
    if features and c > 0.25 and z >= 0.0:
        dx, dz = x / dims.width, z / dims.height
        off = face_offset(dx, dz) * _front_weight(c)
        rx, rf = x, fwd - center
        r = math.hypot(rx, rf) or 1.0
        x, fwd = x + off * rx / r, fwd + off * rf / r
    return x, -fwd, z


def theta_for_x(x: float, z: float, dims: HeadDims = HeadDims()) -> float:
    """顔の側（前の半分）で、表面の x が与えた値になる θ（二分法。|x| が幅を超えれば横）。"""
    lo, hi = 0.0, math.pi / 2
    sign = 1.0 if x >= 0 else -1.0
    for _ in range(40):
        mid = (lo + hi) / 2
        if surface_point(mid, z, dims, features=False)[0] < abs(x):
            lo = mid
        else:
            hi = mid
    return sign * (lo + hi) / 2


def head_rings(dims: HeadDims = HeadDims()) -> list[float]:
    """輪の高さ：首は粗く、顔は細かく（約 3.5 mm）、頭頂の近くは中くらい。"""
    zs = [NECK_BOTTOM_Z + (-0.025 - NECK_BOTTOM_Z) * i / 5 for i in range(5)]
    zs += [-0.025 + 0.025 * i / 6 for i in range(6)]
    zs += [0.0 + 0.170 * i / 48 for i in range(48)]
    zs += [0.170 + (CROWN_Z - 0.003 - 0.170) * i / 11 for i in range(12)]
    return [z * dims.height for z in zs]


def head_mesh(dims: HeadDims = HeadDims(), segments: int = 72) -> tuple[list[Vec3], list[tuple[int, ...]], list[bool]]:
    """頭と首（下は開いている）。戻り値は (頂点, 面, 面が唇か)。頂点は輪ごと、θ の順（0 は前）。最後の頂点は頭頂。"""
    if segments < 8 or segments % 2:
        raise ValueError(f"segments は 8 以上の偶数: {segments}")
    rings = head_rings(dims)
    verts = [surface_point(2 * math.pi * j / segments, z, dims) for z in rings for j in range(segments)]
    top_ring = verts[-segments:]
    verts.append((0.0, sum(v[1] for v in top_ring) / segments, CROWN_Z * dims.height))
    faces: list[tuple[int, ...]] = []
    for i in range(len(rings) - 1):
        a, b = i * segments, (i + 1) * segments
        faces += [(a + j, a + (j + 1) % segments, b + (j + 1) % segments, b + j) for j in range(segments)]
    top, pole = (len(rings) - 1) * segments, len(verts) - 1
    faces += [(top + j, top + (j + 1) % segments, pole) for j in range(segments)]
    lips = [_is_lips(f, verts, dims) for f in faces]
    return verts, faces, lips


def _is_lips(face: tuple[int, ...], verts: list[Vec3], dims: HeadDims) -> bool:
    xs = [verts[i][0] / dims.width for i in face]
    zs = [verts[i][2] / dims.height for i in face]
    ys = [verts[i][1] for i in face]
    half_w, z0, z1 = LIPS
    return max(abs(x) for x in xs) < half_w and z0 < min(zs) and max(zs) < z1 and max(ys) < 0.0


def neck_weights(z: float, dims: HeadDims = HeadDims()) -> list[tuple[str, float]]:
    """首と頭の頂点のウェイト（高さ z で、spine_05 → neck_01 → neck_02 → head へなめらかに移す）。あご先より上は head だけ。"""
    keys = ((-0.115, "spine_05"), (-0.085, "neck_01"), (-0.045, "neck_02"), (0.0, "head"))
    zr = z / dims.height
    if zr >= keys[-1][0]:
        return [("head", 1.0)]
    if zr <= keys[0][0]:
        return [(keys[0][1], 1.0)]
    for (z0, b0), (z1, b1) in zip(keys, keys[1:]):
        if z0 <= zr <= z1:
            t = (zr - z0) / (z1 - z0)
            return [(b0, 1.0 - t), (b1, t)] if 0.0 < t < 1.0 else [((b0 if t <= 0 else b1), 1.0)]
    raise AssertionError("unreachable")
