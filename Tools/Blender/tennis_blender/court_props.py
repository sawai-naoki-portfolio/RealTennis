"""コートサイドの小物の形（純粋な関数。bpy に依存しない）：審判台・球出し機・ボールの缶・クーラーボックス・
サーブ練習の的・電光掲示板（A-PRP-001〜006）。

どれも自分の局所座標で作る：原点＝接地面の中心、+X＝正面（審判台は主審の向く方、球出し機は球の出る方、
掲示板は画面の向く方）、+Z＝上。XZ の面について対称（FBX で Y が反転しても形が変わらない。ART-03 §3）。
UE では、置く位置と向きをアクタで決める。箱と柱だけの仮の形で、仕上げは UE のマテリアルと、必要なら手作業で行う。
"""
from __future__ import annotations

from . import dims as d
from . import materials as mat
from .primitives import Mesh, Vec3, box, join, prism, regular_octagon, regular_polygon, straight_tube

SOCKET_MUZZLE = "SOCKET_muzzle"  # UE のソケット muzzle：球出し機の発射口（球の出る点。+X へ打ち出す）
SIDES = (-1.0, 1.0)              # 左右（Y の負・正）


def _between(a: float, b: float) -> tuple[float, float]:
    return (a, b) if a <= b else (b, a)


def umpire_chair() -> tuple[Mesh, Mesh]:
    """審判台：骨組み（脚・桟・正面のはしご・ひじ掛け・足置き）と、座面・背もたれ。"""
    hx, hy, bar = d.UMPIRE_CHAIR_DEPTH / 2, d.UMPIRE_CHAIR_WIDTH / 2, d.UMPIRE_CHAIR_BAR
    seat = d.UMPIRE_CHAIR_SEAT_HEIGHT
    arm, top = seat + d.UMPIRE_CHAIR_ARM_HEIGHT, seat + d.UMPIRE_CHAIR_BACK_HEIGHT
    foot = seat - d.UMPIRE_CHAIR_FOOTREST_DROP
    seat_front = -hx + bar + d.UMPIRE_CHAIR_SEAT_DEPTH
    between_legs_x, between_legs_y = (-hx + bar, hx - bar), (-hy + bar, hy - bar)
    frame = []
    for side in SIDES:
        ys = _between(side * (hy - bar), side * hy)
        frame.append(box((-hx, -hx + bar), ys, (0.0, top)))                           # 後ろの脚（背もたれの上端まで）
        frame.append(box((hx - bar, hx), ys, (0.0, arm)))                             # 前の脚（ひじ掛けまで）
        frame.append(box(between_legs_x, ys, (arm - bar, arm)))                       # ひじ掛け
        frame.append(box(between_legs_x, ys, (seat / 2 - bar / 2, seat / 2 + bar / 2)))  # 横の桟
    frame.append(box((-hx, -hx + bar), between_legs_y, (seat / 2 - bar / 2, seat / 2 + bar / 2)))  # 後ろの桟
    for i in range(1, d.UMPIRE_CHAIR_RUNGS + 1):                                      # 正面のはしご
        z = seat * i / (d.UMPIRE_CHAIR_RUNGS + 1)
        frame.append(box((hx - bar, hx), between_legs_y, (z - bar / 2, z + bar / 2)))
    frame.append(box((seat_front, hx - bar), between_legs_y, (foot - bar, foot)))     # 足置き
    seat_parts = [box((-hx + bar, seat_front), between_legs_y, (seat - bar, seat)),   # 座面
                  box((-hx, -hx + bar), between_legs_y, (seat + d.UMPIRE_CHAIR_BACK_GAP, top))]  # 背もたれ
    return join(frame), join(seat_parts)


def ball_machine() -> tuple[Mesh, Mesh, Mesh]:
    """球出し機：本体、上の受け箱（上が開いた 4 枚の壁）、黒い部品（後ろの車輪・前の脚・発射口）。"""
    length, width, height = d.BALL_MACHINE_BODY
    hx, hy, z0 = length / 2, width / 2, d.BALL_MACHINE_CLEARANCE
    z1 = z0 + height
    body = box((-hx, hx), (-hy, hy), (z0, z1))

    hop_x, hop_y, wall = d.BALL_MACHINE_HOPPER[0] / 2, d.BALL_MACHINE_HOPPER[1] / 2, d.BALL_MACHINE_HOPPER_WALL
    zs = (z1, z1 + d.BALL_MACHINE_HOPPER[2])
    hopper = [box(_between(side * (hop_x - wall), side * hop_x), (-hop_y, hop_y), zs) for side in SIDES]
    hopper += [box((-hop_x + wall, hop_x - wall), _between(side * (hop_y - wall), side * hop_y), zs) for side in SIDES]

    r, foot = d.BALL_MACHINE_WHEEL_DIAMETER / 2, d.BALL_MACHINE_FOOT
    wheel = regular_octagon(d.BALL_MACHINE_WHEEL_DIAMETER)
    dark = []
    for side in SIDES:
        dark.append(straight_tube((d.BALL_MACHINE_WHEEL_X, side * hy, r),
                                  (d.BALL_MACHINE_WHEEL_X, side * (hy + d.BALL_MACHINE_WHEEL_WIDTH), r), wheel))
        dark.append(box((hx - foot, hx), _between(side * (hy - foot), side * hy), (0.0, z0)))
    dark.append(straight_tube((hx, 0.0, d.BALL_MACHINE_MUZZLE_HEIGHT), muzzle_exit(),
                              regular_octagon(d.BALL_MACHINE_MUZZLE_ACROSS)))
    return body, join(hopper), join(dark)


def muzzle_exit() -> Vec3:
    """球出し機の発射口の先端の中心（球の出る点）。"""
    return d.BALL_MACHINE_BODY[0] / 2 + d.BALL_MACHINE_MUZZLE_LENGTH, 0.0, d.BALL_MACHINE_MUZZLE_HEIGHT


def ball_can() -> tuple[Mesh, Mesh]:
    """ボールの缶（3 個入り）：胴と、少し太いふた。"""
    body_top = d.BALL_CAN_HEIGHT - d.BALL_CAN_LID_HEIGHT
    lid_section = regular_polygon(d.BALL_CAN_DIAMETER + 2 * d.BALL_CAN_LID_OVERHANG, d.BALL_CAN_SIDES)
    return (prism((0.0, 0.0), regular_polygon(d.BALL_CAN_DIAMETER, d.BALL_CAN_SIDES), 0.0, body_top),
            prism((0.0, 0.0), lid_section, body_top, d.BALL_CAN_HEIGHT))


def cooler() -> tuple[Mesh, Mesh, Mesh]:
    """クーラーボックス：本体、少し大きいふた、両端の取っ手。"""
    hx, hy, height = d.COOLER_BODY[0] / 2, d.COOLER_BODY[1] / 2, d.COOLER_BODY[2]
    over = d.COOLER_LID_OVERHANG
    out, handle_w, handle_t = d.COOLER_HANDLE
    handles = [box(_between(side * hx, side * (hx + out)), (-handle_w / 2, handle_w / 2),
                   (d.COOLER_HANDLE_Z, d.COOLER_HANDLE_Z + handle_t)) for side in SIDES]
    return (box((-hx, hx), (-hy, hy), (0.0, height)),
            box((-hx - over, hx + over), (-hy - over, hy + over), (height, height + d.COOLER_LID_HEIGHT)),
            join(handles))


def serve_target() -> tuple[Mesh, Mesh]:
    """サーブ練習の的：外側の枠（4 枚の板）と、中の正方形。"""
    half, inner, zs = d.SERVE_TARGET_SIZE / 2, d.SERVE_TARGET_INNER / 2, (0.0, d.SERVE_TARGET_THICKNESS)
    outer = [box((-half, half), _between(side * inner, side * half), zs) for side in SIDES]
    outer += [box(_between(side * inner, side * half), (-inner, inner), zs) for side in SIDES]
    return join(outer), box((-inner, inner), (-inner, inner), zs)


def scoreboard() -> tuple[Mesh, Mesh, Mesh]:
    """電光掲示板：盤、2 本の支柱、正面（+X）の画面。"""
    hx, hy, height = d.SCOREBOARD_PANEL[0] / 2, d.SCOREBOARD_PANEL[1] / 2, d.SCOREBOARD_PANEL[2]
    z0, post, margin = d.SCOREBOARD_BOTTOM, d.SCOREBOARD_POST / 2, d.SCOREBOARD_SCREEN_MARGIN
    posts = [box((-post, post), (side * d.SCOREBOARD_POST_Y - post, side * d.SCOREBOARD_POST_Y + post), (0.0, z0))
             for side in SIDES]
    return (box((-hx, hx), (-hy, hy), (z0, z0 + height)),
            join(posts),
            box((hx, hx + d.SCOREBOARD_SCREEN_DEPTH), (-hy + margin, hy - margin), (z0 + margin, z0 + height - margin)))


def parts() -> dict[str, list[tuple[str, Mesh]]]:
    """オブジェクトの名前 → (スロット, 部品) の並び。"""
    chair_frame, chair_seat = umpire_chair()
    machine_body, machine_hopper, machine_dark = ball_machine()
    can_body, can_lid = ball_can()
    cooler_body, cooler_lid, cooler_handles = cooler()
    target_outer, target_inner = serve_target()
    board_panel, board_posts, board_screen = scoreboard()
    return {
        "SM_Prop_UmpireChair": [(mat.PROP_FRAME, chair_frame), (mat.PROP_ACCENT, chair_seat)],
        "SM_Prop_BallMachine": [(mat.PROP_ACCENT, machine_body), (mat.PROP_FRAME, machine_hopper),
                                (mat.PROP_DARK, machine_dark)],
        "SM_Prop_BallCan": [(mat.PROP_ACCENT, can_body), (mat.PROP_LIGHT, can_lid)],
        "SM_Prop_Cooler": [(mat.PROP_ACCENT, cooler_body), (mat.PROP_LIGHT, cooler_lid), (mat.PROP_DARK, cooler_handles)],
        "SM_Prop_ServeTarget": [(mat.PROP_ACCENT, target_outer), (mat.PROP_LIGHT, target_inner)],
        "SM_Prop_Scoreboard": [(mat.PROP_DARK, board_panel), (mat.PROP_FRAME, board_posts),
                               (mat.PROP_SCREEN, board_screen)],
    }


def sockets() -> dict[str, tuple[str, Vec3]]:
    """ソケットの Empty の名前 → (親のオブジェクト, 親の座標での位置)。"""
    return {SOCKET_MUZZLE: ("SM_Prop_BallMachine", muzzle_exit())}
