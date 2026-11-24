#include "Ball/TennisBallPredictor.h"

#include "Ball/TennisBallPhysics.h"
#include "TennisConstants.h"
#include "TennisSimLog.h"

namespace
{
	// 時刻をステップの番号に直すときの丸めの余裕（ちょうど境目の時刻を、1 つ前の番号にしない）
	constexpr double IndexTolerance = 1e-9;

	FPredictOptions Validated(const FPredictOptions& Opt)
	{
		FPredictOptions Safe = Opt;
		const FPredictOptions Defaults;
		if (!(Safe.MaxTime > 0.0))  // NaN も弾く
		{
			UE_LOG(LogTennisBall, Warning, TEXT("[Predict] invalid MaxTime=%f, using %f"), Safe.MaxTime, Defaults.MaxTime);
			Safe.MaxTime = Defaults.MaxTime;
		}
		if (Safe.MaxBounces < 1)
		{
			UE_LOG(LogTennisBall, Warning, TEXT("[Predict] invalid MaxBounces=%d, using %d"), Safe.MaxBounces, Defaults.MaxBounces);
			Safe.MaxBounces = Defaults.MaxBounces;
		}
		return Safe;
	}

	bool IsBounce(const FBallEvent& Event)
	{
		return Event.Type == EBallEventType::Bounce;
	}
}

const FBallEvent* FTrajectory::FirstBounce() const
{
	return Events.FindByPredicate(IsBounce);
}

void FTennisBallPredictor::Predict(const FBallState& Start, const FBallSimContext& Ctx, const FPredictOptions& Opt, FTrajectory& Out)
{
	const FPredictOptions Safe = Validated(Opt);
	const int32 MaxSteps = FMath::CeilToInt32(Safe.MaxTime / TennisConst::SimDt - IndexTolerance);

	Out.Seq = Start.Seq;
	Out.Events.Reset();
	Out.States.Reset(MaxSteps + 1);
	Out.States.Add(Start);

	FBallState State = Start;
	int32 NumBounces = 0;
	for (int32 Step = 0; Step < MaxSteps && NumBounces < Safe.MaxBounces; ++Step)
	{
		const int32 EventsBefore = Out.Events.Num();
		TennisBallPhysics::Step(State, TennisConst::SimDt, Ctx, Out.Events);
		Out.States.Add(State);
		for (int32 Index = EventsBefore; Index < Out.Events.Num(); ++Index)
		{
			NumBounces += IsBounce(Out.Events[Index]) ? 1 : 0;
		}
	}
}

bool FTennisBallPredictor::GetStateAt(const FTrajectory& Trajectory, double Time, const FBallSimContext& Ctx, FBallState& Out)
{
	const TArray<FBallState>& States = Trajectory.States;
	if (States.IsEmpty() || !(Time >= States[0].Time) || Time > States.Last().Time)  // NaN も弾く
	{
		return false;
	}

	const double Offset = Time - States[0].Time;
	int32 Index = FMath::Min(FMath::FloorToInt32(Offset / TennisConst::SimDt + IndexTolerance), States.Num() - 1);
	if (States[Index].Time > Time)
	{
		--Index;  // 丸めの余裕で 1 つ先に出たとき
	}

	FBallState State = States[Index];
	const double Remainder = Time - State.Time;
	if (Remainder > 0.0)
	{
		TArray<FBallEvent> Ignored;  // 予測のイベントは Trajectory.Events にある
		TennisBallPhysics::Step(State, Remainder, Ctx, Ignored);
	}
	Out = State;
	return true;
}
