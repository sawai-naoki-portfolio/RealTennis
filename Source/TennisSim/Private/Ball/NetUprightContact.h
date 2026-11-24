// ネットポストとシングルススティック（鉛直の円柱）との衝突（DD-01 §5.1・§5.4）。Step だけが使う。
#pragma once

#include "CoreMinimal.h"
#include "Ball/BallEvents.h"
#include "TennisSimTypes.h"

namespace TennisNetUprightContact
{
	/** Prev → Next（直線で近似）で、ボールがポストかスティックに触れ始める割合（0〜1）。触れなければ値なし。 */
	TOptional<double> ContactFraction(const FBallState& Prev, const FBallState& Next, const FBallSimContext& Ctx);

	/** いちばん近いポストかスティックとの衝突を解く（反発 0.5・摩擦 0.2）。PostHit を加える。 */
	void Resolve(FBallState& State, const FBallSimContext& Ctx, TArray<FBallEvent>& OutEvents);
}
