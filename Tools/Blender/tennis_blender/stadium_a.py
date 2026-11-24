"""センターコート A（A-STD-010）のスタジアムの仮の形（純粋な関数。bpy に依存しない）：観客席・看板の壁・床。

コート局所座標（原点＝ネット中央の地面）。XZ・YZ の面について対称（FBX で Y が反転しても配置が変わらない。ART-03 §3）。
寸法と根拠は ART-01 §4.3。座席・階段・看板の絵などの仕上げは、後の作業で行う。

- 看板の壁：スタジアムのコートの余白（ベースラインの後ろ 8.23 m、横 4.57 m）の外周に沿った低い壁
- 観客席：外周の長方形に沿って断面（列の段・通路・手すり壁）を掃き出す。角は 45° の留め
  - 下の層：辺ごとの 4 つ。四隅は看板の壁と一緒に対角線の向きに切った出入口（上の層の下をくぐって外へ抜ける）
  - 上の層：一周つながる 1 つ。四隅の対角線は縦の通路
  - 縦の通路は隙間にせず、段の面のスロットを M_Stadium_Aisle にした帯にする（後で階段を置く）
- 床：コートの余白（SM_Court_Surface）の外から観客席の外の壁まで。地面（z = 0）に接する壁・観客席の下の面は作らない
- LOD1：段を各列の前の縁を結ぶ斜面にし、縦の通路の帯をなくす（外形とスロットの順は LOD0 と同じ）

看板の壁の裏と下の層の前の面、下の層の屋根と上の層の底は重なるが、どちらも外からは見えない。
"""
from __future__ import annotations

import math

from . import dims as d
from . import materials as mat
from .primitives import Mesh, Vec3, join

Station = tuple[tuple[float, float], tuple[float, float]]  # (内側の線の上の点, 断面の u の向き。留めでは長さ √2)
Profile = list[tuple[float, float, str | None]]           # (u, z, この点から次の点への辺のスロット。None は面を作らない)

SEAT = "seat"  # 段の面の印：縦の通路の帯では M_Stadium_Aisle、ほかは M_Stadium_Seat にする
SLOT_ORDER = (mat.STADIUM_CONCRETE, mat.STADIUM_SEAT, mat.STADIUM_AISLE)  # LOD1 のスロットが LOD0 の先頭と同じ番号になる順
# (外向きの法線, 進む向き)。上から見て時計回りに一周する
SIDES = (((0.0, 1.0), (1.0, 0.0)), ((1.0, 0.0), (0.0, -1.0)), ((0.0, -1.0), (-1.0, 0.0)), ((-1.0, 0.0), (0.0, 1.0)))

LOWER_OFFSET = d.STADIUM_BARRIER_THICKNESS  # 看板の壁の内側の面から、下の層の前の面まで
UPPER_OFFSET = LOWER_OFFSET + d.STADIUM_LOWER_ROWS * d.STADIUM_ROW_DEPTH + d.STADIUM_WALKWAY
OUTER_OFFSET = UPPER_OFFSET + d.STADIUM_UPPER_ROWS * d.STADIUM_ROW_DEPTH + d.STADIUM_PARAPET_THICKNESS
WALKWAY_Z = d.STADIUM_LOWER_FRONT + d.STADIUM_LOWER_ROWS * d.STADIUM_LOWER_RISER  # 横の通路と上の層の底


def _stairs(z_first: float, rows: int, riser: float, lod: int) -> list[tuple[float, float]]:
    """列の段の (u, z)：最前列の前の縁 → 最後列の後ろの縁。LOD1 は各列の前の縁を結ぶ斜面と、最後列の床。"""
    t = d.STADIUM_ROW_DEPTH
    if lod == 0:
        return [(t * (i + k), z_first + riser * i) for i in range(rows) for k in (0, 1)]
    z_last = z_first + riser * (rows - 1)
    return [(0.0, z_first), (t * (rows - 1), z_last), (t * rows, z_last)]


def lower_profile(lod: int) -> Profile:
    """下の層：前の面、列の段、横の通路（上の層の下まで続く屋根）、外の壁。底（z = 0）は作らない。"""
    stairs = _stairs(d.STADIUM_LOWER_FRONT, d.STADIUM_LOWER_ROWS, d.STADIUM_LOWER_RISER, lod)
    back, depth, c = d.STADIUM_LOWER_ROWS * d.STADIUM_ROW_DEPTH, OUTER_OFFSET - LOWER_OFFSET, mat.STADIUM_CONCRETE
    return ([(0.0, 0.0, c)] + [(u, z, SEAT) for u, z in stairs[:-1]] + [(*stairs[-1], c)]
            + [(back, WALKWAY_Z, c), (depth, WALKWAY_Z, c), (depth, 0.0, None)])


def upper_profile(lod: int) -> Profile:
    """上の層：前の面（横の通路から 1 段）、列の段、手すり壁、外の壁、底（出入口の天井）。"""
    r = d.STADIUM_UPPER_RISER
    stairs = _stairs(WALKWAY_Z + r, d.STADIUM_UPPER_ROWS, r, lod)
    back, c = d.STADIUM_UPPER_ROWS * d.STADIUM_ROW_DEPTH, mat.STADIUM_CONCRETE
    top, depth = stairs[-1][1] + d.STADIUM_PARAPET_HEIGHT, back + d.STADIUM_PARAPET_THICKNESS
    return ([(0.0, WALKWAY_Z, c)] + [(u, z, SEAT) for u, z in stairs[:-1]] + [(*stairs[-1], c)]
            + [(back, top, c), (depth, top, c), (depth, WALKWAY_Z, c)])


def barrier_profile() -> Profile:
    t, h, s = d.STADIUM_BARRIER_THICKNESS, d.STADIUM_BARRIER_HEIGHT, mat.STADIUM_SIGNAGE
    return [(0.0, 0.0, s), (0.0, h, s), (t, h, s), (t, 0.0, None)]


def side_stations(n, tau, half_x: float, half_y: float, cut: float, aisles) -> tuple[list[Station], list[bool]]:
    """1 つの辺の断面の位置（両端は角の対角線から cut / 2 だけ離した留め）と、隣どうしの間が縦の通路か。

    half_x・half_y は内側の線の長方形の半分の大きさ、aisles は (長い辺の x, 短い辺の y) の通路の中心（正の側）。
    """
    dist, half, centers = (half_y, half_x, aisles[0]) if n[0] == 0.0 else (half_x, half_y, aisles[1])
    end, w = half - cut / math.sqrt(2.0), d.STADIUM_AISLE_WIDTH
    ts = [-end] + sorted(t for a in centers for t in (a - w / 2, a + w / 2, -a - w / 2, -a + w / 2)) + [end]
    lateral = [(n[0] - tau[0], n[1] - tau[1])] + [n] * (len(ts) - 2) + [(n[0] + tau[0], n[1] + tau[1])]
    stations = [((n[0] * dist + tau[0] * t, n[1] * dist + tau[1] * t), lat) for t, lat in zip(ts, lateral)]
    aisle = [any(abs(abs(a + b) / 2 - c) < w / 2 for c in centers) for a, b in zip(ts, ts[1:])]
    return stations, aisle


def ring_stations(half_x: float, half_y: float, cut: float, aisles) -> tuple[list[Station], list[bool]]:
    """一周する断面の位置と、隣どうしの間が縦の通路か（cut > 0 なら四隅の対角線の帯も通路）。"""
    stations, aisle = [], []
    for n, tau in SIDES:
        s, a = side_stations(n, tau, half_x, half_y, cut, aisles)
        stations += s if cut > 0.0 else s[:-1]  # cut が 0 なら、辺の終わりは次の辺の始まりと同じ点
        aisle += a + [True] if cut > 0.0 else a
    return stations, aisle


def _compact(verts: list[Vec3], faces: list[tuple[int, ...]]) -> Mesh:
    used = sorted({i for f in faces for i in f})
    index = {v: k for k, v in enumerate(used)}
    return [verts[i] for i in used], [tuple(index[i] for i in f) for f in faces]


def sweep(stations: list[Station], aisle: list[bool], profile: Profile, cap: str | None = None) -> dict[str, Mesh]:
    """断面 profile を stations に沿って掃き出した面を、スロットごとに分ける。

    aisle が stations と同じ数なら一周する（最後と最初をつなぐ）。cap は開いた両端のふたのスロット
    （断面の最後の点＝底の後ろの角から扇に分ける。段は外へ向かって高くなるので、扇の三角形は断面の中に入る）。
    """
    k, n = len(profile), len(stations)
    verts = [(cx + lx * u, cy + ly * u, z) for (cx, cy), (lx, ly) in stations for u, z, _ in profile]
    faces: dict[str, list[tuple[int, ...]]] = {}
    for i, is_aisle in enumerate(aisle):
        a, b = i * k, (i + 1) % n * k
        for j, (_, _, kind) in enumerate(profile):
            if kind is not None:
                slot = (mat.STADIUM_AISLE if is_aisle else mat.STADIUM_SEAT) if kind == SEAT else kind
                faces.setdefault(slot, []).append((a + j, b + j, b + (j + 1) % k, a + (j + 1) % k))
    if cap is not None:
        fan = [(k - 1, j, j + 1) for j in range(k - 2)]
        last = (n - 1) * k
        faces.setdefault(cap, []).extend(fan + [tuple(last + v for v in reversed(f)) for f in fan])
    return {slot: _compact(verts, f) for slot, f in faces.items()}


def merge(pieces: list[dict[str, Mesh]], order=SLOT_ORDER) -> list[tuple[str, Mesh]]:
    """部品ごとの (スロット → メッシュ) を、スロットごとに 1 つにまとめる（order の順）。"""
    return [(slot, join([p[slot] for p in pieces if slot in p])) for slot in order if any(slot in p for p in pieces)]


def lower_tier(lod: int) -> list[dict[str, Mesh]]:
    """下の層の 4 つ（辺ごと。四隅は出入口の分だけ離す）。LOD1 は縦の通路の帯を作らない。"""
    half = (d.STADIUM_HALF_X + LOWER_OFFSET, d.STADIUM_HALF_Y + LOWER_OFFSET)
    aisles = (d.STADIUM_LOWER_AISLES_X, d.STADIUM_LOWER_AISLES_Y) if lod == 0 else ((), ())
    return [sweep(*side_stations(n, tau, *half, d.STADIUM_CORNER_GAP, aisles), lower_profile(lod),
                  cap=mat.STADIUM_CONCRETE) for n, tau in SIDES]


def upper_tier(lod: int) -> dict[str, Mesh]:
    """上の層（一周つながる）。LOD1 は四隅の対角線と縦の通路の帯を作らない。"""
    half = (d.STADIUM_HALF_X + UPPER_OFFSET, d.STADIUM_HALF_Y + UPPER_OFFSET)
    if lod == 0:
        stations = ring_stations(*half, d.STADIUM_AISLE_WIDTH, (d.STADIUM_UPPER_AISLES_X, d.STADIUM_UPPER_AISLES_Y))
    else:
        stations = ring_stations(*half, 0.0, ((), ()))
    return sweep(*stations, upper_profile(lod))


def barrier_pieces() -> list[dict[str, Mesh]]:
    """看板の壁の 4 つ（四隅は下の層と同じ対角線で切る）。"""
    return [sweep(*side_stations(n, tau, d.STADIUM_HALF_X, d.STADIUM_HALF_Y, d.STADIUM_CORNER_GAP, ((), ())),
                  barrier_profile(), cap=mat.STADIUM_SIGNAGE) for n, tau in SIDES]


def frame(inner: tuple[float, float], outer: tuple[float, float]) -> Mesh:
    """2 つの長方形（半分の大きさ）の間の平らな額縁（z = 0、上を向く）。"""
    corners = [(1, 1), (1, -1), (-1, -1), (-1, 1)]  # 上から見て時計回り
    verts = [(sx * hx, sy * hy, 0.0) for hx, hy in (inner, outer) for sx, sy in corners]
    return verts, [(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i) for i in range(4)]


def floor() -> list[tuple[str, Mesh]]:
    """コートの余白の外 → 看板の壁（余白の色）、看板の壁 → 外の壁（出入口の床。ほかは観客席の下に隠れる）。"""
    court = (d.HALF_LENGTH + d.RUNOFF_BACK, d.HALF_WIDTH_DOUBLES + d.RUNOFF_SIDE)
    barrier = (d.STADIUM_HALF_X, d.STADIUM_HALF_Y)
    outer = (d.STADIUM_HALF_X + OUTER_OFFSET, d.STADIUM_HALF_Y + OUTER_OFFSET)
    return [(mat.COURT_OUT, frame(court, barrier)), (mat.STADIUM_CONCRETE, frame(barrier, outer))]


def parts() -> dict[str, list[tuple[str, Mesh]]]:
    """オブジェクトの名前 → (スロット, 部品) の並び。"""
    return {
        "SM_Stadium_A_Stands": merge(lower_tier(0) + [upper_tier(0)]),
        "SM_Stadium_A_Stands_LOD1": merge(lower_tier(1) + [upper_tier(1)]),
        "SM_Stadium_A_Barrier": merge(barrier_pieces(), (mat.STADIUM_SIGNAGE,)),
        "SM_Stadium_A_Floor": floor(),
    }


def seat_estimate() -> int:
    """席の数の見積もり：列ごと・辺ごとに、列の中心の線の長さから四隅の切りと縦の通路を引き、席の幅で割る（切り捨て）。"""
    tiers = ((LOWER_OFFSET, d.STADIUM_LOWER_ROWS, d.STADIUM_CORNER_GAP, d.STADIUM_LOWER_AISLES_X, d.STADIUM_LOWER_AISLES_Y),
             (UPPER_OFFSET, d.STADIUM_UPPER_ROWS, d.STADIUM_AISLE_WIDTH, d.STADIUM_UPPER_AISLES_X, d.STADIUM_UPPER_AISLES_Y))
    total = 0
    for offset, rows, cut, aisles_x, aisles_y in tiers:
        for i in range(rows):
            u = offset + d.STADIUM_ROW_DEPTH * (i + 0.5)
            for half, aisles in ((d.STADIUM_HALF_X, aisles_x), (d.STADIUM_HALF_Y, aisles_y)):
                length = 2 * (half + u) - math.sqrt(2.0) * cut - 2 * len(aisles) * d.STADIUM_AISLE_WIDTH
                total += 2 * math.floor(length / d.STADIUM_SEAT_WIDTH)
    return total
