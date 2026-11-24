"""コートの面とラインの矩形の配置（純粋な関数。bpy に依存しない）。

ラインは互いに重ならないように区切る（ベースラインが角を含み、サイドラインはベースラインの内側の縁まで、
サービスラインはシングルスのサイドラインの内側の縁まで）。重なりがないので、1 つのメッシュに
まとめても面が重複しない。
"""
from __future__ import annotations

from dataclasses import dataclass

from . import dims as d


@dataclass(frozen=True)
class Rect:
    """XY 平面の軸に平行な矩形（m）。"""

    name: str
    x_min: float
    x_max: float
    y_min: float
    y_max: float

    def __post_init__(self) -> None:
        if not (self.x_min < self.x_max and self.y_min < self.y_max):
            raise ValueError(f"{self.name}: 幅または奥行きが 0 以下: {self}")

    @property
    def size_x(self) -> float:
        return self.x_max - self.x_min

    @property
    def size_y(self) -> float:
        return self.y_max - self.y_min

    def corners(self) -> list[tuple[float, float]]:
        """4 隅を反時計回り（+Z から見て）で返す。面の法線が +Z になる。"""
        return [
            (self.x_min, self.y_min),
            (self.x_max, self.y_min),
            (self.x_max, self.y_max),
            (self.x_min, self.y_max),
        ]


def court_surface() -> Rect:
    """余白を含むコート全体の外形。"""
    hx = d.HALF_LENGTH + d.RUNOFF_BACK
    hy = d.HALF_WIDTH_DOUBLES + d.RUNOFF_SIDE
    return Rect("surface", -hx, hx, -hy, hy)


def court_inner() -> Rect:
    """ダブルスの区画（ラインの外側の縁まで）。"""
    return Rect("inner", -d.HALF_LENGTH, d.HALF_LENGTH, -d.HALF_WIDTH_DOUBLES, d.HALF_WIDTH_DOUBLES)


def court_lines() -> list[Rect]:
    """すべてのラインの矩形（11 本）。名前の +／- は、中心の座標の符号。"""
    base_inner_x = d.HALF_LENGTH - d.BASELINE_WIDTH
    singles_inner_y = d.HALF_WIDTH_SINGLES - d.LINE_WIDTH
    service_inner_x = d.SERVICE_LINE_X - d.LINE_WIDTH
    half_line = d.LINE_WIDTH / 2
    rects: list[Rect] = []
    for s, tag in ((1, "+"), (-1, "-")):
        rects.append(_x_band(f"baseline{tag}", s, base_inner_x, d.HALF_LENGTH,
                             -d.HALF_WIDTH_DOUBLES, d.HALF_WIDTH_DOUBLES))
        rects.append(_y_band(f"sideline_doubles{tag}", s, d.HALF_WIDTH_DOUBLES - d.LINE_WIDTH,
                             d.HALF_WIDTH_DOUBLES, -base_inner_x, base_inner_x))
        rects.append(_y_band(f"sideline_singles{tag}", s, singles_inner_y, d.HALF_WIDTH_SINGLES,
                             -base_inner_x, base_inner_x))
        rects.append(_x_band(f"service{tag}", s, service_inner_x, d.SERVICE_LINE_X,
                             -singles_inner_y, singles_inner_y))
        rects.append(_x_band(f"center_mark{tag}", s, base_inner_x - d.CENTER_MARK_LENGTH,
                             base_inner_x, -half_line, half_line))
    rects.append(Rect("center_service", -service_inner_x, service_inner_x, -half_line, half_line))
    return rects


def rects_to_quads(rects: list[Rect]) -> tuple[list[tuple[float, float]], list[tuple[int, ...]]]:
    """矩形の列を、XY の頂点と四角形の面に変える（矩形ごとに独立した 4 頂点）。"""
    verts: list[tuple[float, float]] = []
    faces: list[tuple[int, ...]] = []
    for r in rects:
        base = len(verts)
        verts.extend(r.corners())
        faces.append((base, base + 1, base + 2, base + 3))
    return verts, faces


def frame_to_quads(outer: Rect, inner: Rect) -> tuple[list[tuple[float, float]], list[tuple[int, ...]]]:
    """outer から inner を抜いた額縁の形を、8 頂点・4 面にする（法線は +Z）。"""
    if not (outer.x_min < inner.x_min and inner.x_max < outer.x_max
            and outer.y_min < inner.y_min and inner.y_max < outer.y_max):
        raise ValueError(f"inner が outer の内側にない: {inner} / {outer}")
    verts = outer.corners() + inner.corners()
    faces = [(k, (k + 1) % 4, 4 + (k + 1) % 4, 4 + k) for k in range(4)]
    return verts, faces


def _x_band(name: str, sign: int, x_near: float, x_far: float, y_min: float, y_max: float) -> Rect:
    """X が |x_near|〜|x_far| の帯（sign で ±X の側を選ぶ）。"""
    lo, hi = sorted((sign * x_near, sign * x_far))
    return Rect(name, lo, hi, y_min, y_max)


def _y_band(name: str, sign: int, y_near: float, y_far: float, x_min: float, x_max: float) -> Rect:
    """Y が |y_near|〜|y_far| の帯（sign で ±Y の側を選ぶ）。"""
    lo, hi = sorted((sign * y_near, sign * y_far))
    return Rect(name, x_min, x_max, lo, hi)
