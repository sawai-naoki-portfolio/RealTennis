"""ストロークのデータと Chooser の表の場所、仮のストロークの一覧、Notify の時刻の検証（unreal に依存しない。CI で単体テストする）。

DA_Stroke_* は STROKE_FOLDER に置く。表 CHT_StrokeSelect は rebuild_stroke_chooser が DA_Stroke_* から作り直す（DD-03 §5.3）。
仮のストローク（M3 の同期の試作。ART-02）は、Blender の build_stroke_anims.py が Export/ に書き出した AS_*.fbx と、
Notify の時刻の AS_*.json から build_placeholder_strokes が作る。
"""
from __future__ import annotations

import json
import math
from dataclasses import dataclass

STROKE_FOLDER = "/Game/Tennis/Data/Strokes"
STROKE_CHOOSER_NAME = "CHT_StrokeSelect"
STROKE_CHOOSER_PATH = f"{STROKE_FOLDER}/{STROKE_CHOOSER_NAME}"

ANIMATION_FPS = 30  # ART-02 §1
NOTIFY_TRACK = "Tennis"
MAX_NOTIFY_JSON_BYTES = 4096  # 5 つの値だけのファイル。大きいものは読まない
MAX_FRAMES = 600

# build_placeholder_strokes が作ったモンタージュと DA_Stroke_* に付ける目印（メタデータ）。目印のない既存のアセット（本番のもの）には触れない
OWNER_TAG = "TennisTools.Owner"
OWNER_VALUE = "build_placeholder_strokes"


@dataclass(frozen=True)
class PlaceholderStroke:
    animation: str  # AS_*（Export/ の FBX と JSON の名前。取り込んだアニメーションの名前）
    montage: str    # AM_*（アニメーションと同じフォルダに作る）
    data: str       # DA_Stroke_*（STROKE_FOLDER に作る）
    side: str       # 選ばれる条件の向き：unreal.StrokeSide の名前（"FOREHAND"・"BACKHAND"）
    release_lead_time: float  # s（UStrokeData::ReleaseLeadTime。操作仕様 §4.1 の表の値）
    charge_time: float        # s（UStrokeData::ChargeTime。同じ表の値）
    hit_height_min: float | None = None  # m（選ばれる打点の高さの下限：DD-03 §5.3 の高さの変種。None は範囲なし）
    hit_height_max: float | None = None  # m（上限。含まない）


# 高さの変種（DD-03 §5.3、パラメータ表 §4.3）の境界（m）：既定の選手 M01 で焼いた 2 本のスイートスポットの高さの中点を cm に丸めたもの。
# 2026-11-19 に焼いた値：フォア 1.0294・0.8252 → 0.93、バック 1.1202・0.8202 → 0.97。T-STRK-003 を測り直す前に決めた（結果を見て変えない）
FOREHAND_DIP_BOUNDARY = 0.93
BACKHAND_DIP_BOUNDARY = 0.97

# M3 の仮のデータ：フォアとバックの 2 本と、その低く打つ変種 Dip の 2 本。条件は向きと、打点の高さの片側だけの範囲（同じ側は
# 境界を共有する）で、ほかは Any（DD-03 §5.3）。
# 離す時刻の先行時間と溜めの時間は、操作仕様 §4.1 の表のフォアハンド（ドライブ）0.12 s・0.70 s と、バックハンド（片手。BH1 は片手：
# ART-02）0.14 s・0.80 s
PLACEHOLDER_STROKES: tuple[PlaceholderStroke, ...] = (
    PlaceholderStroke("AS_FH_Mid_Drive_Square", "AM_FH_Mid_Drive_Square", "DA_Stroke_FH_Mid_Drive_Square", "FOREHAND", 0.12, 0.70,
                      hit_height_min=FOREHAND_DIP_BOUNDARY),
    PlaceholderStroke("AS_BH1_Mid_Drive_Square", "AM_BH1_Mid_Drive_Square", "DA_Stroke_BH1_Mid_Drive_Square", "BACKHAND", 0.14, 0.80,
                      hit_height_min=BACKHAND_DIP_BOUNDARY),
    PlaceholderStroke("AS_FH_Mid_Drive_Square_Dip", "AM_FH_Mid_Drive_Square_Dip", "DA_Stroke_FH_Mid_Drive_Square_Dip", "FOREHAND", 0.12, 0.70,
                      hit_height_max=FOREHAND_DIP_BOUNDARY),
    PlaceholderStroke("AS_BH1_Mid_Drive_Square_Dip", "AM_BH1_Mid_Drive_Square_Dip", "DA_Stroke_BH1_Mid_Drive_Square_Dip", "BACKHAND", 0.14, 0.80,
                      hit_height_max=BACKHAND_DIP_BOUNDARY),
)


class NotifyTimesError(ValueError):
    """Notify の時刻のファイルが決まりに合わない。"""


@dataclass(frozen=True)
class NotifyTimes:
    impact_time: float   # s
    recover_time: float  # s
    length: float        # s


def read_notify_times(path: str, expected_name: str) -> NotifyTimes:
    """AS_*.json を、決まりの大きさまでだけ読んで検証する（parse_notify_times）。"""
    with open(path, "rb") as file:
        data = file.read(MAX_NOTIFY_JSON_BYTES + 1)
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as error:
        raise NotifyTimesError("UTF-8 として読めない") from error
    return parse_notify_times(text, expected_name)


def parse_notify_times(text: str, expected_name: str) -> NotifyTimes:
    """Blender が書き出した AS_*.json を検証して読む。決まりに合わなければ NotifyTimesError。

    外から来るファイルなので、形・名前・数の範囲（有限、0 < インパクト < リカバー ≤ 長さ、30 fps）を確かめてから使う。
    """
    if len(text.encode("utf-8")) > MAX_NOTIFY_JSON_BYTES:
        raise NotifyTimesError("ファイルが大きすぎる")
    try:
        raw = json.loads(text)
    except (ValueError, RecursionError) as error:  # 深すぎる入れ子も、決まりに合わないものとして扱う
        raise NotifyTimesError(f"JSON として読めない: {type(error).__name__}") from error
    if not isinstance(raw, dict):
        raise NotifyTimesError("JSON の最上位がオブジェクトでない")
    if raw.get("name") != expected_name:
        raise NotifyTimesError(f"name が {expected_name} でない")
    if raw.get("fps") != ANIMATION_FPS:
        raise NotifyTimesError(f"fps が {ANIMATION_FPS} でない")
    frames = raw.get("frames")
    if isinstance(frames, bool) or not isinstance(frames, int) or not 1 <= frames <= MAX_FRAMES:
        raise NotifyTimesError(f"frames が 1〜{MAX_FRAMES} の整数でない")
    times = []
    for key in ("impact_time", "recover_time"):
        value = raw.get(key)
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            raise NotifyTimesError(f"{key} が数でない")
        if isinstance(value, int) and not 0 <= value <= MAX_FRAMES:  # float に直せないほど大きい整数を、直す前に拒む
            raise NotifyTimesError(f"{key} が範囲の外")
        if not math.isfinite(value):
            raise NotifyTimesError(f"{key} が有限の数でない")
        times.append(float(value))
    length = frames / ANIMATION_FPS
    if not 0.0 < times[0] < times[1] <= length:
        raise NotifyTimesError("0 < impact_time < recover_time ≤ 長さ になっていない")
    return NotifyTimes(times[0], times[1], length)
