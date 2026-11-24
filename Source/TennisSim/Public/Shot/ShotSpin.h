// 回転の軸 φ（DD-02 §3.1）：球種の軸を、打つ選手の利き手と横の入力で直す。
#pragma once

#include "CoreMinimal.h"
#include "Shot/ShotTypes.h"

namespace TennisShotSpin
{
	/** 横の入力で傾けられる軸の最大（度。操作仕様 §5）。 */
	inline constexpr double MaxAxisTiltDeg = 15.0;

	/**
	 * 球種の軸 RightHandedAxisDeg（右利きの値、度）から、打つ選手の軸 φ（度）を求める。
	 * 左利きは符号を反転する。横の入力 SideInput（打者から見て右が正）は −1〜1 に収め、右へ倒すほど右へ曲がる向き（φ を減らす向き）に
	 * 最大 15° 傾ける（利き手によらない）。数でない入力は 0 として警告を出す。
	 */
	TENNISSIM_API double AxisDeg(double RightHandedAxisDeg, ETennisHandedness Handedness, double SideInput);
}
