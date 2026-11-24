"""validate_dimensions.py の期待値と比較（純粋な関数。bpy に依存しない）。

期待値は dims から直接計算する（layout などの生成の処理を通さない）。生成の誤りを見つけるため。
"""
from __future__ import annotations

from . import dims as d

TOL = 0.005  # 許容 5 mm（T-CRT-001）

Vec3 = tuple[float, float, float]


def _box(half_x: float, half_y: float, z_min: float, z_max: float) -> tuple[Vec3, Vec3]:
    """XY が原点に対して対称な箱の (最小の角, 最大の角)。"""
    return (-half_x, -half_y, z_min), (half_x, half_y, z_max)


_RACKET_HALF_WIDTH = d.RACKET_FACE_WIDTH / 2 + d.RACKET_FRAME_WIDTH           # ヘッドの外側の縁
_RACKET_HALF_THICK = max(d.RACKET_BEAM, d.RACKET_BUTT_ACROSS) / 2             # いちばん厚いのはバットキャップ
_RACKET_FACE_CENTER_X = d.RACKET_LENGTH - d.RACKET_FRAME_WIDTH - d.RACKET_FACE_LENGTH / 2
_MACHINE_FRONT_X = d.BALL_MACHINE_BODY[0] / 2 + d.BALL_MACHINE_MUZZLE_LENGTH    # 発射口の先端
_MACHINE_HALF_WIDTH = d.BALL_MACHINE_BODY[1] / 2 + d.BALL_MACHINE_WHEEL_WIDTH   # 車輪の外側の面
_MACHINE_HEIGHT = d.BALL_MACHINE_CLEARANCE + d.BALL_MACHINE_BODY[2] + d.BALL_MACHINE_HOPPER[2]
_CAN_RADIUS = d.BALL_CAN_DIAMETER / 2 + d.BALL_CAN_LID_OVERHANG                 # ふたの半径
# スタジアム A：看板の壁の内側の面から外の壁まで、手すり壁の上端の高さ
_STADIUM_DEPTH = (d.STADIUM_BARRIER_THICKNESS + d.STADIUM_ROW_DEPTH * (d.STADIUM_LOWER_ROWS + d.STADIUM_UPPER_ROWS)
                  + d.STADIUM_WALKWAY + d.STADIUM_PARAPET_THICKNESS)
_STADIUM_TOP = (d.STADIUM_LOWER_FRONT + d.STADIUM_LOWER_ROWS * d.STADIUM_LOWER_RISER
                + d.STADIUM_UPPER_ROWS * d.STADIUM_UPPER_RISER + d.STADIUM_PARAPET_HEIGHT)
_STADIUM_OUTER = (d.STADIUM_HALF_X + _STADIUM_DEPTH, d.STADIUM_HALF_Y + _STADIUM_DEPTH)

# オブジェクト名 → ワールドのバウンディングボックス (最小の角, 最大の角)。大きさと原点の位置を同時に確かめる
BOUNDS: dict[str, tuple[Vec3, Vec3]] = {
    "SM_Court_Surface": _box(d.HALF_LENGTH + d.RUNOFF_BACK, d.HALF_WIDTH_DOUBLES + d.RUNOFF_SIDE, 0.0, 0.0),
    "SM_Court_Inner": _box(d.HALF_LENGTH, d.HALF_WIDTH_DOUBLES, 0.0, 0.0),
    "SM_Court_Lines": _box(d.HALF_LENGTH, d.HALF_WIDTH_DOUBLES, 0.0, 0.0),  # 1 mm 浮かせるのは許容の中
    "SM_Net": _box(0.0, d.POST_Y, 0.0, d.NET_HEIGHT_POST),
    "SM_NetBand": _box(d.CORD_DIAMETER / 2, d.POST_Y, d.NET_HEIGHT_CENTER - d.BAND_DEPTH, d.NET_HEIGHT_POST),
    "SM_NetPost": _box(d.POST_DIAMETER / 2, d.POST_Y + d.POST_DIAMETER / 2, 0.0, d.POST_HEIGHT),
    "SM_SinglesStick": _box(d.STICK_DIAMETER / 2, d.STICK_Y + d.STICK_DIAMETER / 2, 0.0, d.STICK_HEIGHT),
    "SM_Ball": _box(d.BALL_RADIUS, d.BALL_RADIUS, -d.BALL_RADIUS, d.BALL_RADIUS),
    "SM_Ball_LOD1": _box(d.BALL_RADIUS, d.BALL_RADIUS, -d.BALL_RADIUS, d.BALL_RADIUS),
    "SM_Ball_Large": _box(d.BALL_RADIUS_LARGE, d.BALL_RADIUS_LARGE, -d.BALL_RADIUS_LARGE, d.BALL_RADIUS_LARGE),
    # ラケット：原点＝グリップエンド、+X＝先端
    "SM_Racket": ((0.0, -_RACKET_HALF_WIDTH, -_RACKET_HALF_THICK),
                  (d.RACKET_LENGTH, _RACKET_HALF_WIDTH, _RACKET_HALF_THICK)),
    # 練習コートの小物（コート局所座標）：フェンスは支柱の外側の面まで、照明柱は灯具の幅まで
    "SM_Practice_Fence": _box(d.FENCE_HALF_X + d.FENCE_POST_ACROSS / 2, d.FENCE_HALF_Y + d.FENCE_POST_ACROSS / 2,
                              0.0, d.FENCE_HEIGHT),
    "SM_Practice_Bench": _box(d.BENCH_LENGTH / 2, d.BENCH_Y + d.BENCH_DEPTH / 2, 0.0, d.BENCH_SEAT_HEIGHT),
    "SM_Practice_LightPole": _box(d.LIGHT_POLE_X + max(d.LIGHT_POLE_ACROSS, d.LIGHT_HEAD_SIZE[1]) / 2,
                                  d.LIGHT_POLE_Y + d.LIGHT_POLE_ACROSS / 2, 0.0, d.LIGHT_POLE_HEIGHT),
    # コートサイドの小物（自分の局所座標：原点＝接地面の中心、+X＝正面）
    "SM_Prop_UmpireChair": _box(d.UMPIRE_CHAIR_DEPTH / 2, d.UMPIRE_CHAIR_WIDTH / 2,
                                0.0, d.UMPIRE_CHAIR_SEAT_HEIGHT + d.UMPIRE_CHAIR_BACK_HEIGHT),
    "SM_Prop_BallMachine": ((-d.BALL_MACHINE_BODY[0] / 2, -_MACHINE_HALF_WIDTH, 0.0),
                            (_MACHINE_FRONT_X, _MACHINE_HALF_WIDTH, _MACHINE_HEIGHT)),
    "SM_Prop_BallCan": _box(_CAN_RADIUS, _CAN_RADIUS, 0.0, d.BALL_CAN_HEIGHT),
    "SM_Prop_Cooler": _box(d.COOLER_BODY[0] / 2 + d.COOLER_HANDLE[0], d.COOLER_BODY[1] / 2 + d.COOLER_LID_OVERHANG,
                           0.0, d.COOLER_BODY[2] + d.COOLER_LID_HEIGHT),
    "SM_Prop_ServeTarget": _box(d.SERVE_TARGET_SIZE / 2, d.SERVE_TARGET_SIZE / 2, 0.0, d.SERVE_TARGET_THICKNESS),
    "SM_Prop_Scoreboard": ((-d.SCOREBOARD_PANEL[0] / 2, -d.SCOREBOARD_PANEL[1] / 2, 0.0),
                           (d.SCOREBOARD_PANEL[0] / 2 + d.SCOREBOARD_SCREEN_DEPTH, d.SCOREBOARD_PANEL[1] / 2,
                            d.SCOREBOARD_BOTTOM + d.SCOREBOARD_PANEL[2])),
    # スタジアム A（コート局所座標）：観客席は外の壁まで（LOD1 も同じ外形）、看板の壁は裏の面まで、床は外の壁まで
    "SM_Stadium_A_Stands": _box(*_STADIUM_OUTER, 0.0, _STADIUM_TOP),
    "SM_Stadium_A_Stands_LOD1": _box(*_STADIUM_OUTER, 0.0, _STADIUM_TOP),
    "SM_Stadium_A_Barrier": _box(d.STADIUM_HALF_X + d.STADIUM_BARRIER_THICKNESS, d.STADIUM_HALF_Y + d.STADIUM_BARRIER_THICKNESS,
                                 0.0, d.STADIUM_BARRIER_HEIGHT),
    "SM_Stadium_A_Floor": _box(*_STADIUM_OUTER, 0.0, 0.0),
}

# ソケットの Empty → (親, 親の座標での位置)。回転は単位（DD-03 §7）
SOCKETS: dict[str, tuple[str, Vec3]] = {
    "SOCKET_racket_sweetspot": ("SM_Racket", (_RACKET_FACE_CENTER_X - d.RACKET_SWEETSPOT_OFFSET, 0.0, 0.0)),
    "SOCKET_grip": ("SM_Racket", (d.RACKET_GRIP_SOCKET_X, 0.0, 0.0)),
    "SOCKET_muzzle": ("SM_Prop_BallMachine", (_MACHINE_FRONT_X, 0.0, d.BALL_MACHINE_MUZZLE_HEIGHT)),  # 球の出る点
}

# ラケットの上限（Appendix II。RES-01 §3）：全長・全幅・打球面の長さ・打球面の幅
RACKET_LIMITS = (0.737, 0.317, 0.394, 0.292)

# オブジェクト名 → 面の数（分割の仕様。ART-01 §2・ART-03 §4）
FACE_COUNTS: dict[str, int] = {
    "SM_Net": d.NET_SEGMENTS_Y * d.NET_SEGMENTS_Z,
    "SM_Ball": d.BALL_SEGMENTS * d.BALL_RINGS,
    "SM_Ball_LOD1": d.BALL_LOD1_SEGMENTS * d.BALL_LOD1_RINGS,
    "SM_Ball_Large": d.BALL_SEGMENTS * d.BALL_RINGS,
}

# ネットの上辺の高さ (y, 高さ)：中央・シングルススティック・ポスト（Rule 1）
NET_TOP_HEIGHTS: list[tuple[float, float]] = [
    (0.0, d.NET_HEIGHT_CENTER),
    (d.STICK_Y, d.NET_HEIGHT_POST), (-d.STICK_Y, d.NET_HEIGHT_POST),
    (d.POST_Y, d.NET_HEIGHT_POST), (-d.POST_Y, d.NET_HEIGHT_POST),
]

# 三角形の数の上限（ART-01 §1 の予算）：分類 → (オブジェクト名, 上限)
TRI_BUDGETS: dict[str, tuple[tuple[str, ...], int]] = {
    "ネット": (("SM_Net", "SM_NetBand", "SM_NetPost", "SM_SinglesStick"), 4000),
    "ボール": (("SM_Ball",), 1000),
    "ラケット": (("SM_Racket",), 6000),
    "練習コートの小物": (("SM_Practice_Fence", "SM_Practice_Bench", "SM_Practice_LightPole"), 10000),
    "コートサイドの小物": (("SM_Prop_UmpireChair", "SM_Prop_BallMachine", "SM_Prop_BallCan", "SM_Prop_Cooler",
                   "SM_Prop_ServeTarget", "SM_Prop_Scoreboard"), 10000),
    # スタジアム A の仮の形（ART-01 §4.3。ART-01 §1 の構造は制限なしだが、仮の形は小さく保つ）
    "スタジアム A（LOD0）": (("SM_Stadium_A_Stands", "SM_Stadium_A_Barrier", "SM_Stadium_A_Floor"), 20000),
    "スタジアム A（LOD1）": (("SM_Stadium_A_Stands_LOD1",), 2000),
}


def court_line_edges() -> tuple[list[float], list[float]]:
    """ラインの縁の X 座標と Y 座標（ITF の寸法から。T-CRT-001 で頂点と照合する）。"""
    xs = [d.HALF_LENGTH, d.HALF_LENGTH - d.BASELINE_WIDTH,
          d.HALF_LENGTH - d.BASELINE_WIDTH - d.CENTER_MARK_LENGTH,
          d.SERVICE_LINE_X, d.SERVICE_LINE_X - d.LINE_WIDTH]
    ys = [d.HALF_WIDTH_DOUBLES, d.HALF_WIDTH_DOUBLES - d.LINE_WIDTH,
          d.HALF_WIDTH_SINGLES, d.HALF_WIDTH_SINGLES - d.LINE_WIDTH, d.LINE_WIDTH / 2]
    return _mirror(xs), _mirror(ys)


def within(actual: float, expected: float, tol: float = TOL) -> bool:
    return abs(actual - expected) <= tol


def unmatched(actual: list[float], expected: list[float], tol: float = TOL) -> list[float]:
    """actual のうち、expected のどれとも tol 以内で一致しない値。"""
    return [a for a in actual if not any(within(a, e, tol) for e in expected)]


def _mirror(values: list[float]) -> list[float]:
    return sorted(values + [-v for v in values])
