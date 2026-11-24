"""仮のストロークの動き（純粋な関数。bpy に依存しない）：キーの姿勢、フレームごとの補間、2 本の骨の IK、ラケットの向き。

M3 の同期の試作に使う仮のデータ（ART-02。モーションキャプチャに置き換える）。その場で振る（ルートは動かさない：DD-03 §6）。
座標は選手の座標（m）：+X＝選手の左、−Y＝前（ネットの方）、+Z＝上、原点＝足元の中央。右利きで作る（左利きはミラー）。
"""
from __future__ import annotations

import math
from dataclasses import dataclass, replace

Vec3 = tuple[float, float, float]

FPS = 30
GRIP_TO_SWEET_SPOT = 0.421  # m（ラケットの grip → racket_sweetspot。ART-01 §2：48.1 cm − 6.0 cm）

# 手のソケット racket_attach_r（ART-01 §3.1）を、Blender の骨の座標で表したもの。UE の骨の座標 (x, y, z) は
# Blender の骨の座標 (x, −y, z) に当たる（2026-11-12 に hand_r で確かめた）。位置は m
SOCKET_LOCATION_IN_HAND: Vec3 = (-0.065, -0.030, 0.0)
SOCKET_RACKET_AXIS_IN_HAND: Vec3 = (-math.sin(math.radians(25.0)), 0.0, math.cos(math.radians(25.0)))  # グリップ → 先端
SOCKET_FACE_NORMAL_IN_HAND: Vec3 = (0.0, -1.0, 0.0)                                                    # 手のひらの法線


@dataclass(frozen=True)
class StrokeKey:
    frame: int
    pelvis_drop: float   # m（腰を落とす量。ひざは足の IK で曲がる）
    pelvis_yaw: float    # 度（+Z まわり。負＝右を向く）
    chest_yaw: float     # 度（腰に対する胸のひねり。背骨に等分する）
    sweet_spot: Vec3     # スイートスポットの位置
    racket_dir: Vec3     # グリップ → 先端の向き
    face_normal: Vec3    # 打球面の法線（racket_dir に直交する成分だけを使う）
    off_wrist: Vec3      # 利き手でない方の手首の位置


@dataclass(frozen=True)
class StrokeMotion:
    name: str            # UE のアニメーションの名前（AS_*。モンタージュは AM_*：ART-02 §1）
    keys: tuple[StrokeKey, ...]
    impact_frame: int    # Notify「Impact」
    recover_frame: int   # Notify「RecoverStart」

    @property
    def last_frame(self) -> int:
        return self.keys[-1].frame


def add(a: Vec3, b: Vec3, s: float = 1.0) -> Vec3:
    return a[0] + b[0] * s, a[1] + b[1] * s, a[2] + b[2] * s


def sub(a: Vec3, b: Vec3) -> Vec3:
    return a[0] - b[0], a[1] - b[1], a[2] - b[2]


def dot(a: Vec3, b: Vec3) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a: Vec3, b: Vec3) -> Vec3:
    return a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]


def length(a: Vec3) -> float:
    return math.sqrt(dot(a, a))


def normalized(a: Vec3) -> Vec3:
    n = length(a)
    if n < 1e-9:
        raise ValueError("長さ 0 のベクトルは正規化できない")
    return a[0] / n, a[1] / n, a[2] / n


def _hermite(p0: float, p1: float, m0: float, m1: float, t: float) -> float:
    t2, t3 = t * t, t * t * t
    return (2 * t3 - 3 * t2 + 1) * p0 + (t3 - 2 * t2 + t) * m0 + (-2 * t3 + 3 * t2) * p1 + (t3 - t2) * m1


def _interp(frames: list[int], values: list[float], frame: float) -> float:
    """キーの間をなめらかにつなぐ（Catmull-Rom。端のキーでは速さ 0）。範囲の外は端の値。"""
    if frame <= frames[0]:
        return values[0]
    if frame >= frames[-1]:
        return values[-1]
    i = max(k for k in range(len(frames) - 1) if frames[k] <= frame)
    h = frames[i + 1] - frames[i]

    def slope(k: int) -> float:
        if k == 0 or k == len(frames) - 1:
            return 0.0
        return (values[k + 1] - values[k - 1]) / (frames[k + 1] - frames[k - 1])

    return _hermite(values[i], values[i + 1], slope(i) * h, slope(i + 1) * h, (frame - frames[i]) / h)


def sample(motion: StrokeMotion, frame: float) -> StrokeKey:
    """フレーム（小数も可）の姿勢。向きは補間の後で正規化する。"""
    frames = [k.frame for k in motion.keys]

    def scalar(name: str) -> float:
        return _interp(frames, [getattr(k, name) for k in motion.keys], frame)

    def vector(name: str) -> Vec3:
        return tuple(_interp(frames, [getattr(k, name)[a] for k in motion.keys], frame) for a in range(3))

    return StrokeKey(frame=int(round(frame)), pelvis_drop=scalar("pelvis_drop"), pelvis_yaw=scalar("pelvis_yaw"),
                     chest_yaw=scalar("chest_yaw"), sweet_spot=vector("sweet_spot"), racket_dir=normalized(vector("racket_dir")),
                     face_normal=normalized(vector("face_normal")), off_wrist=vector("off_wrist"))


def racket_frame(racket_dir: Vec3, face_normal: Vec3) -> tuple[Vec3, Vec3, Vec3]:
    """ラケットの直交する 3 軸（先端の向き・幅の向き・面の法線）。法線は、先端の向きに直交する成分だけを使う。"""
    x = normalized(racket_dir)
    z = normalized(sub(face_normal, tuple(c * dot(face_normal, x) for c in x)))
    return x, cross(z, x), z


def grip_position(key: StrokeKey) -> Vec3:
    """スイートスポットから、ラケットの向きに沿って戻ったグリップ（ソケット grip）の位置。"""
    return add(key.sweet_spot, key.racket_dir, -GRIP_TO_SWEET_SPOT)


def two_bone_ik(root: Vec3, target: Vec3, upper: float, lower: float, pole: Vec3) -> tuple[Vec3, Vec3]:
    """2 本の骨（長さ upper・lower）で root から target へ届かせる。戻り値は (関節の位置, 先端の位置)。

    届かない target は、腕を伸ばしきる手前（99.9%）まで寄せる。近すぎる target は、たためる限界まで離す。
    関節は pole の向き（root → target に直交する成分）へ曲げる。
    """
    to_target = sub(target, root)
    distance = length(to_target)
    if distance < 1e-9:
        raise ValueError("root と target が同じ位置")
    u = normalized(to_target)
    reach = min(max(distance, abs(upper - lower) * 1.001 + 1e-6), (upper + lower) * 0.999)
    along = (upper * upper - lower * lower + reach * reach) / (2 * reach)
    height = math.sqrt(max(upper * upper - along * along, 0.0))
    side = sub(pole, tuple(c * dot(pole, u) for c in u))
    if length(side) < 1e-6:
        raise ValueError("pole が root → target と平行")
    joint = add(add(root, u, along), normalized(side), height)
    return joint, add(root, u, reach)


def notify_times(motion: StrokeMotion) -> dict[str, float | int | str]:
    """UE の側でモンタージュに Notify を置くための値（書き出しの FBX の隣に JSON で置く）。時間は s。"""
    return {"name": motion.name, "fps": FPS, "frames": motion.last_frame,
            "impact_time": motion.impact_frame / FPS, "recover_time": motion.recover_frame / FPS}


_READY_OFF = (0.20, -0.30, 1.05)

# フォアハンド（中くらいの高さ・ドライブ・スクエア）：構え → 肩を回して引く → 下から前へ振る → インパクト → 左肩の上へ振り抜く → 構え
FOREHAND = StrokeMotion(
    name="AS_FH_Mid_Drive_Square",
    keys=(
        StrokeKey(0, 0.08, 0.0, 0.0, (-0.10, -0.62, 1.20), (0.35, -0.55, 0.75), (-0.8, -0.5, 0.0), _READY_OFF),
        StrokeKey(14, 0.10, -20.0, -55.0, (-0.42, 0.62, 1.55), (-0.15, 0.45, 0.88), (-0.9, 0.2, -0.3), (-0.10, -0.42, 1.25)),
        StrokeKey(19, 0.12, -15.0, -40.0, (-0.85, 0.30, 0.82), (-0.75, 0.45, -0.45), (-0.3, -0.8, -0.4), (0.15, -0.40, 1.20)),
        StrokeKey(23, 0.10, -5.0, -10.0, (-0.82, -0.38, 1.02), (-0.92, -0.25, 0.05), (0.2, -0.95, 0.1), (0.28, -0.25, 1.15)),
        StrokeKey(31, 0.06, 10.0, 45.0, (0.30, -0.45, 1.75), (0.55, 0.05, 0.85), (0.6, -0.7, -0.3), (0.30, -0.15, 1.25)),
        StrokeKey(38, 0.06, 10.0, 50.0, (0.48, 0.02, 1.72), (0.45, 0.55, 0.60), (0.8, -0.3, -0.4), (0.28, -0.18, 1.25)),
        StrokeKey(52, 0.08, 0.0, 0.0, (-0.10, -0.62, 1.20), (0.35, -0.55, 0.75), (-0.8, -0.5, 0.0), _READY_OFF),
    ),
    impact_frame=23, recover_frame=38)

# 片手の仮のバックハンド（BH1。中くらいの高さ・ドライブ・スクエア）：左へ肩を回して引く → 前へ振る → インパクト → 右上へ振り抜く
BACKHAND = StrokeMotion(
    name="AS_BH1_Mid_Drive_Square",
    keys=(
        StrokeKey(0, 0.08, 0.0, 0.0, (-0.10, -0.62, 1.20), (0.35, -0.55, 0.75), (-0.8, -0.5, 0.0), _READY_OFF),
        StrokeKey(14, 0.10, 20.0, 60.0, (0.55, 0.45, 1.25), (0.45, 0.60, 0.55), (0.8, -0.5, 0.2), (0.22, 0.10, 1.10)),
        StrokeKey(19, 0.12, 15.0, 45.0, (0.80, 0.10, 0.85), (0.80, 0.30, -0.40), (0.3, -0.9, -0.2), (0.25, -0.05, 1.05)),
        StrokeKey(23, 0.10, 8.0, 15.0, (0.72, -0.42, 1.00), (0.92, -0.30, 0.05), (-0.3, -0.95, 0.1), (0.30, 0.05, 1.05)),
        StrokeKey(31, 0.06, -5.0, -35.0, (-0.55, -0.50, 1.65), (-0.55, -0.10, 0.80), (-0.6, -0.7, 0.3), (0.32, 0.10, 1.05)),
        StrokeKey(38, 0.06, -5.0, -40.0, (-0.70, -0.10, 1.70), (-0.55, 0.35, 0.75), (-0.8, -0.3, 0.4), (0.30, 0.08, 1.05)),
        StrokeKey(52, 0.08, 0.0, 0.0, (-0.10, -0.62, 1.20), (0.35, -0.55, 0.75), (-0.8, -0.5, 0.0), _READY_OFF),
    ),
    impact_frame=23, recover_frame=38)


def dipped(motion: StrokeMotion, name: str, drop: float, pelvis_share: float = 0.5) -> StrokeMotion:
    """motion を低く打つ変種（ART-02 §1 の Dip。DD-03 §5.3 の高さの変種）。構えのキー（最初と最後）はそのままにし、ほかのキーの
    スイートスポットと利き手でない方の手首を drop（m）だけ下げ、腰を drop × pelvis_share だけ落とす（腕だけで下げない）。"""
    first, last = motion.keys[0].frame, motion.keys[-1].frame
    keys = tuple(key if key.frame in (first, last) else replace(
        key,
        pelvis_drop=key.pelvis_drop + drop * pelvis_share,
        sweet_spot=add(key.sweet_spot, (0.0, 0.0, -drop)),
        off_wrist=add(key.off_wrist, (0.0, 0.0, -drop * pelvis_share)),
    ) for key in motion.keys)
    return StrokeMotion(name=name, keys=keys, impact_frame=motion.impact_frame, recover_frame=motion.recover_frame)


# 低く打つ変種（Dip）：既定の選手（M01）で焼いたインパクトのスイートスポットの高さが、目標の 0.82 m（パラメータ表 §4.3）の前後になる量だけ
# 下げる（2026-11-19 に Manny で測った：フォア 0.810 m、バック 0.783 m。M01 で焼くと、フォア +0.01 m、バック +0.04 m ほど高い）。
# バックは腕が伸びきって届かないので、腰を drop と同じだけ落とす（仮の動き。M9 のモーションキャプチャで置き換える）
FOREHAND_DIP = dipped(FOREHAND, "AS_FH_Mid_Drive_Square_Dip", drop=0.21)
BACKHAND_DIP = dipped(BACKHAND, "AS_BH1_Mid_Drive_Square_Dip", drop=0.30, pelvis_share=1.0)

MOTIONS: tuple[StrokeMotion, ...] = (FOREHAND, BACKHAND, FOREHAND_DIP, BACKHAND_DIP)
