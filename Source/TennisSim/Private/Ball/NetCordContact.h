// 白帯（ネットコード）との衝突（DD-01 §5.2）。Step だけが使う（モジュールの外に出さない）。
// 白帯は X = 0・高さ NetHeightAt(Y) を通る水平の円柱（ポストの内側だけ）として扱う。
#pragma once

#include "CoreMinimal.h"
#include "Ball/BallEvents.h"
#include "TennisSimTypes.h"

namespace TennisNetCordContact
{
	/**
	 * Prev → Next（直線で近似）で、ボールが白帯に触れ始める位置までの割合（0〜1）。触れなければ値なし。
	 * ステップの途中の接触も見逃さない（速い球のすり抜けを防ぐ）。離れていく球は数えない。
	 */
	TOptional<double> ContactFraction(const FBallState& Prev, const FBallState& Next, const FBallSimContext& Ctx);

	/** 白帯に触れている状態を解く（反発 0.30・摩擦 0.4 の撃力）。NetTouch（Cord）を加え、bNetTouched を立てる。 */
	void Resolve(FBallState& State, const FBallSimContext& Ctx, TArray<FBallEvent>& OutEvents);
}
