// インパクトの判定（DD-03 §6.4 の 3〜5）：Engine に依存しない純粋な関数。インパクトの時刻 T の体の位置、スイートスポットの位置、
// 打てる距離か。位置はコート局所（m）、時間は s（ボールのシミュレーション時刻）、角度は rad。
#pragma once

#include "CoreMinimal.h"
#include "Shot/ShotTypes.h"
#include "Stroke/TennisStrokeTiming.h"

namespace TennisStrokeImpact
{
	/** 焼いた値（右利きの体で焼いた、X が前・Y が右・Z が地面からの高さ。m）を利き手に合わせる：左利きは Y を反転する */
	TENNISSIM_API FVector HandedOffset(const FVector& SweetSpotOffset, ETennisHandedness Handedness);

	/**
	 * T の体の水平の位置：このフレームの始まりの位置 FrameStartPos に、速度 Velocity ×（T − FrameStartTime）を足した位置
	 * （選手の移動は、ボールとストロークの後に動くので、インパクトを知った時点の位置はフレームの始まりのもの）。
	 */
	TENNISSIM_API FVector2D BodyAt(const FVector2D& FrameStartPos, const FVector2D& Velocity, double FrameStartTime, double T);

	/**
	 * スイートスポットの位置：ヨー YawRad を向いた、水平の位置 BodyPos の体の、焼いた値 Offset（HandedOffset のもの）の位置。
	 * 水平は TennisStrokeTiming::BodyTarget のちょうど逆、高さは Offset.Z。
	 */
	TENNISSIM_API FVector SweetSpotAt(const FVector2D& BodyPos, double YawRad, const FVector& Offset);

	/** 打てるか：スイートスポットとボールの中心の距離 Distance が ImpactReach 以下（境界は打つ。FR-STRK-006）。数でない距離は打てない */
	TENNISSIM_API bool IsWithinReach(double Distance, const FStrokeSyncParams& Params);
}
