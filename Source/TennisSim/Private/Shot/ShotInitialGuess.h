// ショットソルバの初期値（DD-02 §7.2 の 2）：抗力とマグヌスを入れた放物線の近似で、打ち出しの速さと仰角を見積もる。
#pragma once

#include "CoreMinimal.h"
#include "Shot/ShotTypes.h"
#include "TennisSimTypes.h"

namespace TennisShotInitialGuess
{
	/** 打ち出しの速さ（m/s）と仰角（rad）。 */
	struct FGuess
	{
		double Speed = 0.0;
		double Elevation = 0.0;
	};

	/**
	 * 方位 Azimuth（rad）へ打つときの初期値。範囲には収めない（ソルバが収める）。
	 * NetClearance：ネットの上の余裕 ConstraintValue と飛距離の 2 つを満たす (s, θ)。解けなければ、または Apex なら、リクエストの仰角で飛距離に届く速さ。
	 * FixedSpeed：リクエストの速さと仰角のまま。
	 */
	FGuess Estimate(const FShotRequest& Request, double Azimuth, double ConstraintValue, const FBallSimContext& Ctx);
}
