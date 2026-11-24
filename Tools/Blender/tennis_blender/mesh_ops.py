"""メッシュの処理（純粋な関数。bpy に依存しない）：隣り合い・縮める・縁・平らにする・ウェイトを混ぜる・縁の輪と筒。

頂点は (x, y, z) の m、面は頂点の番号の組。仮の選手（ART-01 A-CHR-004・005）の服と体を整えるのに使う。
"""
from __future__ import annotations

import math

Vec3 = tuple[float, float, float]


def adjacency(vertex_count: int, faces: list[tuple[int, ...]]) -> list[list[int]]:
    neighbors: list[set[int]] = [set() for _ in range(vertex_count)]
    for f in faces:
        for i, a in enumerate(f):
            b = f[(i + 1) % len(f)]
            neighbors[a].add(b)
            neighbors[b].add(a)
    return [sorted(n) for n in neighbors]


def erode(mask: list[bool], adj: list[list[int]], rings: int) -> list[bool]:
    """mask を rings 回だけ内へ縮める（隣に外の頂点がある頂点を外す）。"""
    cur = list(mask)
    for _ in range(rings):
        cur = [cur[i] and all(cur[j] for j in adj[i]) for i in range(len(cur))]
    return cur


def faces_inside(faces: list[tuple[int, ...]], mask: list[bool]) -> list[bool]:
    return [all(mask[i] for i in f) for f in faces]


def border_neighbors(faces: list[tuple[int, ...]], keep: list[bool]) -> dict[int, list[int]]:
    """残す面の集まりの縁の辺（残す面 1 つだけが使う辺）で、縁の頂点どうしの隣り合い。"""
    count: dict[tuple[int, int], int] = {}
    for f, k in zip(faces, keep):
        if k:
            for i, a in enumerate(f):
                b = f[(i + 1) % len(f)]
                e = (min(a, b), max(a, b))
                count[e] = count.get(e, 0) + 1
    nb: dict[int, list[int]] = {}
    for (a, b), c in count.items():
        if c == 1:
            nb.setdefault(a, []).append(b)
            nb.setdefault(b, []).append(a)
    return nb


def region_adjacency(adj: list[list[int]], mask: list[bool], faces: list[tuple[int, ...]]) -> list[list[int]]:
    """平らにするときの隣り合い：mask の中だけ。殻の縁の頂点は縁に沿ってだけ（縁は内へ縮まず、縁の線がなめらかになる）。"""
    border = border_neighbors(faces, faces_inside(faces, mask))
    return [(border[i] if i in border else [j for j in nb if mask[j]]) if mask[i] else [] for i, nb in enumerate(adj)]


def relax(pos: list[Vec3], adj: list[list[int]], movable: list[bool], lam: float = 0.5) -> list[Vec3]:
    """ラプラスの平滑化の 1 回：動かせる頂点を、隣の平均へ lam だけ寄せる。"""
    out = list(pos)
    for i, nb in enumerate(adj):
        if movable[i] and nb:
            c = [sum(pos[j][a] for j in nb) / len(nb) for a in range(3)]
            out[i] = tuple(pos[i][a] + lam * (c[a] - pos[i][a]) for a in range(3))
    return out


def drape(base: list[Vec3], normals: list[Vec3], faces: list[tuple[int, ...]], adj: list[list[int]], mask: list[bool],
          min_dist: list[float], steps: int, lam: float = 0.5) -> list[Vec3]:
    """mask の頂点を平らにしながら、元の位置 base から自分の法線の向きに min_dist より内へ入らないように押し戻す。
    溝の底の頂点は持ち上がり、出っ張りの頂点は min_dist までしか下がらない（溝をまたいで、高いところに乗る）。
    縁は縁に沿ってだけ平らにする（region_adjacency）。"""
    region_adj = region_adjacency(adj, mask, faces)
    cur = [tuple(b[a] + n[a] * max(0.0, d) for a in range(3)) if k else b
           for b, n, d, k in zip(base, normals, min_dist, mask)]
    for _ in range(steps):
        cur = relax(cur, region_adj, mask, lam)
        for i, k in enumerate(mask):
            if k:
                b, n, p = base[i], normals[i], cur[i]
                d = sum((p[a] - b[a]) * n[a] for a in range(3))
                if d < min_dist[i]:
                    cur[i] = tuple(p[a] + n[a] * (min_dist[i] - d) for a in range(3))
    return cur


def blend_weights(neighbors: list[tuple[float, list[tuple[str, float]]]], max_influences: int = 4) -> list[tuple[str, float]]:
    """近くの頂点のウェイトを、距離の逆数で混ぜる（作り直した体へ写す）。重い順に max_influences 個、合計 1。"""
    total: dict[str, float] = {}
    for dist, weights in neighbors:
        k = 1.0 / (dist + 1e-4)
        for bone, w in weights:
            total[bone] = total.get(bone, 0.0) + k * w
    top = sorted(total.items(), key=lambda bw: -bw[1])[:max_influences]
    s = sum(w for _, w in top)
    if s <= 0.0:
        raise ValueError("ウェイトのない頂点だけが近くにある")
    return [(b, w / s) for b, w in top]


def boundary_loops(faces: list[tuple[int, ...]], keep: list[bool]) -> list[list[int]]:
    """残す面の集まりの縁を、頂点の輪にする。"""
    nb = border_neighbors(faces, keep)
    loops, seen = [], set()
    for start in nb:
        if start in seen:
            continue
        loop, prev, cur = [start], None, start
        seen.add(start)
        while True:
            nxt = next((v for v in nb[cur] if v != prev and v not in seen), None)
            if nxt is None:
                break
            loop.append(nxt)
            seen.add(nxt)
            prev, cur = cur, nxt
        loops.append(loop)
    return loops


def smooth_loop(points: list[Vec3], steps: int, lam: float = 0.5) -> list[Vec3]:
    """閉じた点の列をなめらかにする（1 次元のラプラス）。"""
    cur = list(points)
    n = len(cur)
    for _ in range(steps):
        cur = [tuple(cur[i][a] + lam * ((cur[i - 1][a] + cur[(i + 1) % n][a]) / 2 - cur[i][a]) for a in range(3))
               for i in range(n)]
    return cur


def resample_loop(points: list[Vec3], count: int) -> list[Vec3]:
    """閉じた点の列を、長さに沿って等間隔に count 点にする。"""
    n = len(points)
    seg = [math.dist(points[i], points[(i + 1) % n]) for i in range(n)]
    total = sum(seg)
    out, i, acc = [], 0, 0.0
    for k in range(count):
        target = total * k / count
        while acc + seg[i] < target:
            acc += seg[i]
            i += 1
        t = (target - acc) / (seg[i] or 1.0)
        a, b = points[i], points[(i + 1) % n]
        out.append(tuple(a[c] + (b[c] - a[c]) * t for c in range(3)))
    return out


def loop_tube(loop: list[Vec3], center: Vec3, radius: float, sides: int = 8) -> tuple[list[Vec3], list[tuple[int, ...]]]:
    """閉じた点の列に沿った輪の筒（襟のリブ）。断面は、列の向きと、center から外への向きで決める。面は外を向く。"""
    n = len(loop)
    verts: list[Vec3] = []
    for i, p in enumerate(loop):
        tx, ty, tz = (loop[(i + 1) % n][a] - loop[i - 1][a] for a in range(3))
        tl = math.sqrt(tx * tx + ty * ty + tz * tz) or 1.0
        tx, ty, tz = tx / tl, ty / tl, tz / tl
        ox, oy, oz = p[0] - center[0], p[1] - center[1], 0.0
        d = ox * tx + oy * ty
        ox, oy = ox - d * tx, oy - d * ty
        ol = math.hypot(ox, oy) or 1.0
        out = (ox / ol, oy / ol, oz)
        up = (ty * out[2] - tz * out[1], tz * out[0] - tx * out[2], tx * out[1] - ty * out[0])
        for j in range(sides):
            a = 2 * math.pi * j / sides
            verts.append(tuple(p[c] + radius * (math.cos(a) * out[c] + math.sin(a) * up[c]) for c in range(3)))
    faces = [(i * sides + j, i * sides + (j + 1) % sides, ((i + 1) % n) * sides + (j + 1) % sides, ((i + 1) % n) * sides + j)
             for i in range(n) for j in range(sides)]
    return verts, faces
