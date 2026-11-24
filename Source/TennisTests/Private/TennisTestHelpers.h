// テストに共通の打ち出しと飛ばし方（DRY：同じ打ち出しの値と、最初のバウンドまで飛ばす手順を 1 か所に置く）。
#pragma once

#include "CoreMinimal.h"
#include "Ball/TennisBallPhysics.h"
#include "TennisConstants.h"

namespace TennisTest
{
	/** ベースラインの後ろからのトップスピンのラリーの球（参照シミュレーションの BENCH の rally_fh に近い打ち出し）。 */
	inline FBallState RallyShot(double Time = 0.0)
	{
		FBallState State;
		State.Pos = FVector(-11.9, 0.0, 0.95);
		State.Vel = FVector(34.0, 0.0, 6.0);
		State.Spin = FVector(0.0, 240.0, 0.0);
		State.Time = Time;
		return State;
	}

	/** イベントの中で、Type の最初のもの（なければ nullptr）。 */
	inline const FBallEvent* FirstOf(const TArray<FBallEvent>& Events, EBallEventType Type)
	{
		return Events.FindByPredicate([Type](const FBallEvent& Event) { return Event.Type == Type; });
	}

	/** イベントの中の Type の数。 */
	inline int32 CountOf(const TArray<FBallEvent>& Events, EBallEventType Type)
	{
		return Events.FilterByPredicate([Type](const FBallEvent& Event) { return Event.Type == Type; }).Num();
	}

	/** 最初のバウンドまで（最長 MaxSeconds 秒）本番と同じ Step で進め、その間のイベントを返す。State は進んだ状態になる。 */
	inline TArray<FBallEvent> FlyToFirstBounce(FBallState& State, const FBallSimContext& Ctx = FBallSimContext(), double MaxSeconds = 2.0)
	{
		TArray<FBallEvent> Events;
		const int32 MaxSteps = FMath::CeilToInt32(MaxSeconds / TennisConst::SimDt);
		for (int32 Step = 0; Step < MaxSteps && FirstOf(Events, EBallEventType::Bounce) == nullptr; ++Step)
		{
			TennisBallPhysics::Step(State, TennisConst::SimDt, Ctx, Events);
		}
		return Events;
	}
}
