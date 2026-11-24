// ネットの面（X = 0）を越えるときの扱い（DD-01 §5.1・§5.3、ACT-01）。Step だけが使う（モジュールの外に出さない）。
#pragma once

#include "CoreMinimal.h"
#include "Ball/BallEvents.h"
#include "TennisSimTypes.h"

namespace TennisNetPlaneContact
{
	/** Prev → Next でボールの中心がネットの面を越えたら、その位置までの割合（0〜1）。越えなければ値なし。 */
	TOptional<double> CrossingFraction(const FBallState& Prev, const FBallState& Next);

	/**
	 * ネットの面の上にいる状態を解く：本体（網）に当たれば跳ね返し、そうでなければ通す。イベントを OutEvents に加える。
	 * ポストの外を回る球には何もしない（Rule 25 c）。白帯との衝突は、ここではなく NetCordContact が扱う。
	 */
	void Resolve(FBallState& State, const FBallSimContext& Ctx, TArray<FBallEvent>& OutEvents);
}
