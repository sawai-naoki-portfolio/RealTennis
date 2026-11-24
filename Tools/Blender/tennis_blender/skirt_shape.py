"""仮の選手のスカートの形とウェイト（純粋な関数。bpy に依存しない）。座標は Blender のワールド（m）。"""
from __future__ import annotations

import math

Vec3 = tuple[float, float, float]


def skirt_rings(body: list[Vec3], center: tuple[float, float], z_top: float, z_bottom: float, rings: int,
                segments: int, clearance: float, flare: float, pleats: int) -> list[list[Vec3]]:
    """スカートの輪（上から下）。各輪の半径は、その高さの体の外形（角度ごとの最大の半径）に clearance を足したもの。
    下へ向けて前の輪より狭くせず、裾は flare だけ広げる。プリーツは半径の山（pleats 個）。"""
    out: list[list[Vec3]] = []
    prev = [0.0] * segments
    band = abs(z_top - z_bottom) / max(1, rings - 1) * 0.75 + 0.01
    for k in range(rings):
        s = k / (rings - 1)
        z = z_top + (z_bottom - z_top) * s
        radius = [0.0] * segments
        for p in body:
            if abs(p[2] - z) <= band:
                a = math.atan2(p[1] - center[1], p[0] - center[0])
                j = int(round(a / (2 * math.pi) * segments)) % segments
                radius[j] = max(radius[j], math.hypot(p[0] - center[0], p[1] - center[1]))
        radius = _fill_gaps(radius)
        ring = []
        for j in range(segments):
            r = max(prev[j], radius[j] + clearance) if k else radius[j] + clearance
            prev[j] = r
            pleat = 0.006 * s * ((j * pleats / segments) % 1.0)  # 片ひだ（のこぎりの歯）
            rr = r + flare * s * s + pleat
            a = 2 * math.pi * j / segments
            ring.append((center[0] + rr * math.cos(a), center[1] + rr * math.sin(a), z))
        out.append(ring)
    return out


def _fill_gaps(radius: list[float]) -> list[float]:
    """体の点がなかった角度を、両隣の値で埋める。"""
    n = len(radius)
    known = [i for i in range(n) if radius[i] > 0.0]
    if not known:
        raise ValueError("スカートの高さに体の点がない")
    out = list(radius)
    for i in range(n):
        if out[i] == 0.0:
            out[i] = max(radius[known[min(range(len(known)), key=lambda k: min((i - known[k]) % n, (known[k] - i) % n))]], 0.0)
    return out


def skirt_weights(x: float, s: float) -> list[tuple[str, float]]:
    """スカートのウェイト：腰（s＝0）は pelvis だけ、裾（s＝1）は pelvis 0.4 と、その側の太もも 0.6。"""
    leg = 0.6 * s
    t = min(1.0, max(0.0, (x + 0.04) / 0.08))
    left = t * t * (3 - 2 * t)  # 選手の左は +X
    out = [("pelvis", 1.0 - leg)]
    if leg * left > 0.0:
        out.append(("thigh_l", leg * left))
    if leg * (1.0 - left) > 0.0:
        out.append(("thigh_r", leg * (1.0 - left)))
    return out
