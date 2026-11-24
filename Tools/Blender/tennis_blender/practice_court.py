"""練習コートの小物の形（純粋な関数。bpy に依存しない）：フェンス・ベンチ・照明柱（A-STD-001）。

すべてコート局所座標で作る（原点＝ネット中央の地面）。UE ではコートと同じく原点に置くだけでそろう。
どれも XZ・YZ の面について対称（FBX で Y が反転しても配置が変わらない。ART-03 §3）。

- フェンス：余白の外周に沿った支柱（正八角柱。角の間を 10 ft 以下に等分）、上の横桟、網（面。UE で Masked・Two Sided）
- ベンチ：ネットのポストの後ろ、フェンスの手前に左右 1 つずつ（座面と 2 枚の脚）
- 照明柱：サービスラインの延長のフェンスの外に 4 本。灯具はコートの方を向く
"""
from __future__ import annotations

import math

from . import dims as d
from . import materials as mat
from .primitives import Mesh, box, join, prism, regular_octagon, straight_tube


def side_positions(half: float) -> list[float]:
    """-half → +half を、間隔が FENCE_POST_SPACING_MAX 以下になるように等分した点（両端を含む）。"""
    n = math.ceil(2 * half / d.FENCE_POST_SPACING_MAX)
    return [-half + 2 * half * i / n for i in range(n + 1)]


def fence_posts_xy() -> list[tuple[float, float]]:
    """フェンスの支柱の位置（角を含む。重複なし）。"""
    hx, hy = d.FENCE_HALF_X, d.FENCE_HALF_Y
    xs, ys = side_positions(hx), side_positions(hy)
    return [(x, y) for x in xs for y in (-hy, hy)] + [(x, y) for x in (-hx, hx) for y in ys[1:-1]]


def fence_posts() -> Mesh:
    section = regular_octagon(d.FENCE_POST_ACROSS)
    return join([prism(xy, section, 0.0, d.FENCE_HEIGHT) for xy in fence_posts_xy()])


def fence_rails() -> Mesh:
    """上の横桟（支柱の上端より少し下。支柱の上の面と同じ高さにしない）。"""
    hx, hy, z = d.FENCE_HALF_X, d.FENCE_HALF_Y, d.FENCE_HEIGHT - d.FENCE_RAIL_ACROSS
    section = regular_octagon(d.FENCE_RAIL_ACROSS)
    corners = [(-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy)]
    return join([straight_tube((*a, z), (*b, z), section) for a, b in zip(corners, corners[1:] + corners[:1])])


def fence_net() -> Mesh:
    """網の 4 枚の面（地面から横桟の中心まで。面はコートの内側を向く）。"""
    hx, hy, top = d.FENCE_HALF_X, d.FENCE_HALF_Y, d.FENCE_HEIGHT - d.FENCE_RAIL_ACROSS
    corners = [(-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy)]  # 反時計回り
    verts = [(x, y, z) for x, y in corners for z in (0.0, top)]
    faces = [(2 * i, 2 * i + 1, 2 * j + 1, 2 * j) for i, j in zip(range(4), [1, 2, 3, 0])]
    return verts, faces


def benches() -> Mesh:
    """ベンチ 2 つ（y = ±BENCH_Y）：座面と、端から少し内側の 2 枚の脚（座面の下の面までの高さ）。"""
    half_l, half_d = d.BENCH_LENGTH / 2, d.BENCH_DEPTH / 2
    seat_bottom = d.BENCH_SEAT_HEIGHT - d.BENCH_SEAT_THICKNESS
    outer, t = half_l - d.BENCH_LEG_INSET, d.BENCH_LEG_THICKNESS
    parts = []
    for y in (-d.BENCH_Y, d.BENCH_Y):
        ys = (y - half_d, y + half_d)
        parts.append(box((-half_l, half_l), ys, (seat_bottom, d.BENCH_SEAT_HEIGHT)))
        parts += [box(xs, ys, (0.0, seat_bottom)) for xs in ((-outer, -outer + t), (outer - t, outer))]
    return join(parts)


def light_poles() -> tuple[Mesh, Mesh]:
    """照明柱 4 本と腕（1 つ目）、灯具（2 つ目）。灯具は柱からコートの方（y = 0 の方）へ出し、上面は柱の上端にそろえる。"""
    h, half_a = d.LIGHT_POLE_HEIGHT, d.LIGHT_ARM_ACROSS / 2
    depth, width, thick = d.LIGHT_HEAD_SIZE
    z_mid = h - thick / 2
    poles, heads = [], []
    for x in (-d.LIGHT_POLE_X, d.LIGHT_POLE_X):
        for side in (-1.0, 1.0):
            y = side * d.LIGHT_POLE_Y
            root = y - side * d.LIGHT_ARM_LENGTH
            tip = root - side * depth
            poles.append(prism((x, y), regular_octagon(d.LIGHT_POLE_ACROSS), 0.0, h))
            poles.append(box((x - half_a, x + half_a), tuple(sorted((root, y))), (z_mid - half_a, z_mid + half_a)))
            heads.append(box((x - width / 2, x + width / 2), tuple(sorted((tip, root))), (h - thick, h)))
    return join(poles), join(heads)


def parts() -> dict[str, list[tuple[str, Mesh]]]:
    """オブジェクトの名前 → (スロット, 部品) の並び。"""
    pole, head = light_poles()
    return {
        "SM_Practice_Fence": [(mat.FENCE_POST, fence_posts()), (mat.FENCE_POST, fence_rails()),
                              (mat.FENCE_NET, fence_net())],
        "SM_Practice_Bench": [(mat.BENCH, benches())],
        "SM_Practice_LightPole": [(mat.LIGHT_POLE, pole), (mat.LIGHT_HEAD, head)],
    }
