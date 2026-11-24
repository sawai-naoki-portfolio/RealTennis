// フレームの時間を、固定ステップ（240 Hz）の数に直す（DD-01 §3.3、ARCH-03 D-3）。
// ゲームの結果は、ここが返すステップの数だけで進める。フレームの時間（DeltaTime）を物理に直接使わない。
#pragma once

#include "CoreMinimal.h"

struct TENNISSIM_API FFixedStepClock
{
	static constexpr double MaxFrameTime = 0.1;  // s（これより長いフレームは切り詰める。止まった後に一度に進めすぎない）

	/**
	 * フレームの時間（s）と時間の倍率を加え、このフレームで進めるステップの数を返す。
	 * 負・NaN の値は 0 として扱う。フレームの時間は MaxFrameTime で切り詰めてから倍率をかける。
	 */
	int32 Advance(double FrameTime, double TimeScale = 1.0);

	/** 最後のステップから、次のステップまでの割合（0 以上 1 未満）。描画の補間だけに使う。 */
	double Alpha() const;

	/** まだステップにしていない時間（s。0 以上 SimDt 未満）。結果は変えず、ステップの間の位置を外へ渡すときだけに使う。 */
	double Remainder() const { return Accumulator; }

private:
	double Accumulator = 0.0;  // s（まだステップにしていない時間）
};
