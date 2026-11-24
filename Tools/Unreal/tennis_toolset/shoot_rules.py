"""shoot_ball の入力の検証（unreal に依存しない純粋な関数。CI で単体テストする：ARCH-04 §4）。"""
from __future__ import annotations

import math
import re

SHOT_TYPE_NAME = re.compile(r"^DA_Shot_[A-Za-z]+$")  # 球種のアセット名（パスの区切りや拡張子を受け付けない）
MAX_TARGET_X_M = 30.0  # コートの長さの半分（11.885 m）の外の余裕を含めた範囲
MAX_TARGET_Y_M = 20.0


def validate_shoot_ball(shot_type: str, target_x_m: float, target_y_m: float) -> None:
    """shoot_ball の引数を確かめる。受け付けなければ ValueError。"""
    if not SHOT_TYPE_NAME.match(shot_type):
        raise ValueError(f"shot_type must be an asset name like DA_Shot_Topspin: {shot_type!r}")
    for name, value, limit in (("target_x_m", target_x_m, MAX_TARGET_X_M), ("target_y_m", target_y_m, MAX_TARGET_Y_M)):
        if not math.isfinite(value) or abs(value) > limit:
            raise ValueError(f"{name} must be a finite number within ±{limit} m: {value!r}")
