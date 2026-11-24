#include "Ball/TennisBallSimComponent.h"

#include "Ball/TennisBallDebugDraw.h"
#include "Ball/TennisBallPhysics.h"
#include "Core/TennisDebugSettings.h"
#include "Court/TennisCourt.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "TennisConstants.h"
#include "TennisSimLog.h"
#include "VisualLogger/VisualLogger.h"

UTennisBallSimComponent::UTennisBallSimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;  // フレームの最初に進める。選手はこの後に処理する（ARCH-01 §5）
}

void UTennisBallSimComponent::BeginPlay()
{
	Super::BeginPlay();
	if (Court == nullptr && GetWorld() != nullptr)
	{
		TActorIterator<ATennisCourt> It(GetWorld());
		Court = It ? *It : nullptr;
	}
	if (Court == nullptr)
	{
		UE_LOG(LogTennisBall, Warning, TEXT("[Ball] %s: no ATennisCourt in the level; the ball will not move on screen"), *GetNameSafe(GetOwner()));
	}
}

void UTennisBallSimComponent::Launch(const FBallState& LaunchState)
{
	const double Now = State.Time;
	State = LaunchState;
	bInFlight = true;

	const double Late = Now - LaunchState.Time;
	if (Late > 0.0)
	{
		if (Late > LateLaunchWarning)
		{
			UE_LOG(LogTennisBall, Warning, TEXT("[Launch] seq=%d is %.1f ms late"), LaunchState.Seq, Late * 1000.0);
		}
		StepEvents.Reset();
		TennisBallPhysics::AdvanceTo(State, Now, Context, StepEvents);  // 早送り（DD-01 §3.4）
		Broadcast(StepEvents);
	}
	PrevState = State;
	ActualPath.Reset();
	ActualPath.Add(State.Pos);

	FTennisBallPredictor::Predict(LaunchState, Context, FPredictOptions(), Prediction);
	if (Court != nullptr)
	{
		UE_VLOG_LOCATION(GetOwner(), LogTennisBall, Log, Court->LocalMetersToWorld(LaunchState.Pos), 6.0f, FColor::White,
			TEXT("Launch seq=%d"), LaunchState.Seq);
	}
	UE_LOG(LogTennisBall, Log, TEXT("[Launch] seq=%d t=%.4f pos=%s vel=%s spin=%s"), LaunchState.Seq, LaunchState.Time,
		*LaunchState.Pos.ToString(), *LaunchState.Vel.ToString(), *LaunchState.Spin.ToString());
}

bool UTennisBallSimComponent::GetStateAt(double Time, FBallState& Out) const
{
	return FTennisBallPredictor::GetStateAt(Prediction, Time, Context, Out);
}

void UTennisBallSimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// フレームの時間はステップの数に直すだけに使う（物理には固定ステップだけ：ARCH-03 D-3）。時計と通し番号は、飛んでいない間も
	// 進める（ストロークの時計：DD-03 §6.1）。状態と時刻は、飛んでいる間だけ進める
	const int32 Steps = Clock.Advance(DeltaTime, GetDefault<UTennisDebugSettings>()->GetTimeScale());
	StepCount += Steps;
	if (!bInFlight)
	{
		return;
	}
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		StepOnce();
	}
	UpdateOwnerLocation();
	DrawDebug();
}

void UTennisBallSimComponent::DrawDebug() const
{
	const int32 Mode = GetDefault<UTennisDebugSettings>()->GetTrajectoryMode();  // tennis.Debug.Trajectory
	if (Mode <= 0 || Court == nullptr)
	{
		return;
	}
	TennisBallDebugDraw::DrawPrediction(GetWorld(), *Court, Prediction);
	if (Mode >= 2)
	{
		TennisBallDebugDraw::DrawActualPath(GetWorld(), *Court, ActualPath);
	}
}

void UTennisBallSimComponent::StepOnce()
{
	PrevState = State;
	StepEvents.Reset();
	TennisBallPhysics::Step(State, TennisConst::SimDt, Context, StepEvents);
	if (ActualPath.Num() < Prediction.States.Num())  // 予測と同じ長さまで（デバッグの描画：モード 2）
	{
		ActualPath.Add(State.Pos);
	}
	Broadcast(StepEvents);
}

void UTennisBallSimComponent::Broadcast(const TArray<FBallEvent>& Events)
{
	for (const FBallEvent& Event : Events)
	{
		UE_LOG(LogTennisBall, Log, TEXT("[%s] t=%.4f pos=%s"), LexToString(Event.Type), Event.Time, *Event.Pos.ToString());
		if (Court != nullptr)
		{
			UE_VLOG_LOCATION(GetOwner(), LogTennisBall, Log, Court->LocalMetersToWorld(Event.Pos), 6.0f, FColor::Red,
				TEXT("%s t=%.3f"), LexToString(Event.Type), Event.Time);
		}
		OnBallEvent.Broadcast(Event);
	}
}

void UTennisBallSimComponent::UpdateOwnerLocation() const
{
	AActor* Owner = GetOwner();
	if (Court == nullptr || Owner == nullptr)
	{
		return;
	}
	const FVector RenderPos = FMath::Lerp(PrevState.Pos, State.Pos, Clock.Alpha());  // 見た目だけの補間
	Owner->SetActorLocation(Court->LocalMetersToWorld(RenderPos));
}
