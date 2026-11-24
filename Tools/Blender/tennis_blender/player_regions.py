"""選手の体の頂点の分け方と、形の処理（純粋な関数。bpy に依存しない）。

体（仮の選手は Manny・Quinn）の頂点を、いちばん重いウェイトの骨と位置で、服（シャツ・ショートパンツ・靴・靴下・リストバンド）と
消す部分（元の頭と首）に分ける。座標は Blender のワールド（m。選手は −Y を向く。足の裏が z＝0）。MetaHuman の体（服の殻の元：
ART-03 §6 の手順 5）は補助の骨を持つので、いちばん重い骨を manny_bone で Manny の骨にしてから分ける。
"""
from __future__ import annotations

import math
from dataclasses import dataclass

from .player_specs import SHOE_TOP, WRIST_BAND, Outfit

Vec3 = tuple[float, float, float]

TOP, BOTTOM, SHOE, SOCK, WRIST, REMOVE, SMOOTH = "top", "bottom", "shoe", "sock", "wrist", "remove", "smooth"
COLLAR_R = 0.078      # 襟ぐりの半径（首の軸から、m）。前は Outfit.neck_dip だけ広げる。この内の体は消す（作り直した首がふさぐ）
HIDE_MARGIN = 0.020   # 襟ぐりの外のこの幅の体も消す（シャツの襟の下で、首の付け根とのすき間を見せない）
WAIST_ABOVE_PELVIS = 0.06

SPINE = frozenset(f"spine_0{i}" for i in range(1, 6))
HEAD_NECK = frozenset({"head", "neck_01", "neck_02"})


def _side_bones(stem: str, twists: int = 2) -> frozenset[str]:
    names = set()
    for s in ("l", "r"):
        names.add(f"{stem}_{s}")
        names.update(f"{stem}_twist_0{i}_{s}" for i in range(1, twists + 1))
    return frozenset(names)


CLAVICLE = frozenset({"clavicle_l", "clavicle_r"})
UPPERARM, LOWERARM = _side_bones("upperarm"), _side_bones("lowerarm")
THIGH, CALF = _side_bones("thigh"), _side_bones("calf")
FOOT = frozenset({"foot_l", "foot_r", "ball_l", "ball_r"})
FINGERS = ("thumb", "index", "middle", "ring", "pinky")
LIMBS = UPPERARM | LOWERARM | THIGH | CALF
MANNY = SPINE | HEAD_NECK | CLAVICLE | LIMBS | FOOT | frozenset({"root", "pelvis", "hand_l", "hand_r"})  # 分け方が知っている骨


@dataclass(frozen=True)
class Landmarks:
    bones: dict[str, Vec3]           # 骨の付け根の位置（ワールド、m）
    neck_axis: tuple[float, float]   # 首の軸の (x, y)（作り直す首の中心）
    collar_z: float                  # これより上だけ襟ぐりで切る（m。胸の下のほうを切らない）
    collar_r: float = COLLAR_R       # 襟ぐりの後ろの半径（m。体ごと。前は Outfit.neck_dip だけ広げる）


def manny_bone(bone: str, parent_of: dict[str, str | None]) -> str:
    """骨を、親をたどって最初に当たる MANNY の骨にする（MetaHuman の補助の骨 *_twistCor_*・calf_knee_*・clavicle_pec_* や、
    手と足の指を、分け方が知っている骨にする）。たどり着かなければ ""。parent_of は骨 → 親の骨（根は None）。"""
    seen: set[str] = set()
    while bone and bone not in MANNY and bone not in seen:
        seen.add(bone)
        bone = parent_of.get(bone) or ""
    return bone if bone in MANNY else ""


def is_hand(bone: str) -> bool:
    """手（手のひらと指）の骨。手は元の体の細かい形をそのまま使う（ボクセルで作り直すと指がくっつく）。"""
    return bone in ("hand_l", "hand_r") or bone.startswith(FINGERS)


def segment_t(p: Vec3, a: Vec3, b: Vec3) -> float:
    """点 p を、a → b の線分に投影したときの位置（a が 0、b が 1。外にも出る）。"""
    d = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
    dd = sum(c * c for c in d) or 1.0
    return sum((p[i] - a[i]) * d[i] for i in range(3)) / dd


def _side(bone: str) -> str:
    return bone[-1]


def _limb_t(p: Vec3, bone: str, lm: Landmarks, start: str, end: str) -> float:
    s = _side(bone)
    return segment_t(p, lm.bones[f"{start}_{s}"], lm.bones[f"{end}_{s}"])


def collar_radius(p: Vec3, lm: Landmarks, neck_dip: float) -> float:
    """襟ぐりの半径：後ろは lm.collar_r、前ほど neck_dip だけ広い（前の襟ぐりが深い）。"""
    dx, dy = p[0] - lm.neck_axis[0], p[1] - lm.neck_axis[1]
    r = math.hypot(dx, dy) or 1.0
    front = max(0.0, -dy / r)  # 選手は −Y を向く
    return lm.collar_r + neck_dip * front * front


def neck_radius(p: Vec3, lm: Landmarks) -> float:
    return math.hypot(p[0] - lm.neck_axis[0], p[1] - lm.neck_axis[1])


def near_neck(p: Vec3, lm: Landmarks, neck_dip: float, margin: float = 0.0) -> bool:
    return p[2] > lm.collar_z and neck_radius(p, lm) < collar_radius(p, lm, neck_dip) + margin


def classify(p: Vec3, bone: str, lm: Landmarks, outfit: Outfit) -> frozenset[str]:
    """頂点の部分（重なってよい：腰はシャツとショートパンツの両方）。bone はいちばん重いウェイトの骨。"""
    tags: set[str] = set()
    pelvis_z = lm.bones["pelvis"][2]
    if bone in HEAD_NECK or ((bone in SPINE or bone in CLAVICLE) and near_neck(p, lm, outfit.neck_dip)):
        return frozenset({REMOVE})
    if (bone in SPINE or bone in CLAVICLE or bone == "pelvis" or bone in THIGH) and p[2] >= pelvis_z + outfit.hem:
        tags.add(TOP)  # 腰はシャツとショートパンツを重ねる（境目に穴を作らない）
    if bone in UPPERARM and outfit.sleeve is not None and _limb_t(p, bone, lm, "upperarm", "lowerarm") <= outfit.sleeve:
        tags.add(TOP)
    if ((bone == "pelvis" or bone in SPINE) and p[2] <= pelvis_z + WAIST_ABOVE_PELVIS) or (
            bone in THIGH and _limb_t(p, bone, lm, "thigh", "calf") <= outfit.shorts):
        tags.add(BOTTOM)
    if bone in FOOT or (bone in CALF and p[2] < SHOE_TOP):
        tags.add(SHOE)
    elif bone in CALF and p[2] < outfit.sock_top:
        tags.add(SOCK)
    if bone in LOWERARM or bone in ("hand_l", "hand_r"):
        t = _limb_t(p, bone, lm, "lowerarm", "hand")
        if WRIST_BAND[0] <= t <= WRIST_BAND[1]:
            tags.add(WRIST)
    if bone in LIMBS and not tags & {TOP, BOTTOM, SHOE}:
        tags.add(SMOOTH)
    return frozenset(tags)
