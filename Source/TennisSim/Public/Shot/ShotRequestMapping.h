// 球種のデータから、ソルバのリクエストを作る（DD-02 §4.1）。能力値・品質・球威の再利用・姿勢による補正は M4 で加える。
#pragma once

#include "CoreMinimal.h"
#include "Shot/ShotTypeParams.h"
#include "Shot/ShotTypes.h"

namespace TennisShotRequestMapping
{
	/** 溜めの速さの曲線の指数（DD-02 §4：s = lerp(MinSpeed, MaxSpeed, Charge^0.8)） */
	inline constexpr double ChargeSpeedExponent = 0.8;

	/**
	 * 球種 Params を、打点 Origin・狙い Target・インパクトの時刻 Time・溜め Charge でリクエストにする。
	 * 溜めは 0〜1 に収め、数でない値は 0 として警告を出す。条件の目標は溜めで補間し、速さを固定する球種の速さは溜めの曲線で決める。
	 * 回転の軸は、利き手と横の入力 SideInput で直す（TennisShotSpin::AxisDeg）。
	 */
	TENNISSIM_API FShotRequest Make(const FShotTypeParams& Params, const FVector& Origin, const FVector& Target, double Time,
		double Charge, ETennisHandedness Handedness, double SideInput);

	/**
	 * インパクトでの溜め（0〜1。DD-03 §6.4 の 6、ST-05）：離していれば（離した時刻 − 溜めの始まり）÷ ChargeTime を 0〜1 に収めたもの。
	 * 離していない（押したままインパクトが来た：入力が自動で離す）か、自動で離した印があれば 1（溜めの最大。利用者の判断、2026-11-14）。
	 * 溜めの始まりがない・ChargeTime が正でない・時刻が数でないときは、警告を出して 1。
	 */
	TENNISSIM_API double ChargeAtImpact(const FShotIntent& Intent, double ChargeTime);
}
