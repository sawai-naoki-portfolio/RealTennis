#include "Player/TennisAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "Core/TennisDebugSettings.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisMovementComponent.h"
#include "Player/TennisStrokeComponent.h"
#include "Stroke/TennisStrokeStepper.h"

void UTennisAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const ATennisCharacter* Character = Cast<ATennisCharacter>(TryGetPawnOwner());
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (Character == nullptr || Mesh == nullptr)
	{
		return;  // エディタのプレビューなど：軌跡と重みは既定のまま
	}
	const UTennisStrokeComponent* Stroke = Character->GetStroke();
	if (Stroke != nullptr && Stroke->IsStroking())
	{
		LowerBodyLocomotionAlpha = static_cast<float>(TennisStrokeStep::LowerBodyLocomotionAlpha(Stroke->GetStepper(), Stroke->Sync));
	}
	else if (GetCurrentActiveMontage() != nullptr)
	{
		LowerBodyLocomotionAlpha = 0.0f;  // ストロークの外で動いているモンタージュ（RecoverStart の後の構えへの戻り、Slot で直に再生したとき）は全身
	}
	else if (!IsAnyMontagePlaying())
	{
		LowerBodyLocomotionAlpha = 1.0f;  // モンタージュがない：どちらの姿勢もロコモーションなので、切り替えは見えない
	}
	// ほかは、ブレンドアウト中のモンタージュだけがある：前のフレームの値を保つ（取り消し・終わりで腰と脚が急に替わらない。DD-03 §6.5）

	AnimTime += FMath::Max(static_cast<double>(DeltaSeconds), 0.0);
	const double Now = AnimTime;
	const FVector MeshNow = Mesh->GetComponentLocation();
	const FQuat Facing = Mesh->GetComponentQuat();  // M3：どの標本も今の体の向き（DD-03 §6.5）
	History.Add({ Now, MeshNow });
	const double Interval = FMath::Max(static_cast<double>(SampleInterval), 0.01);
	const int32 PastCount = FMath::FloorToInt32(FMath::Max(static_cast<double>(HistorySeconds), 0.0) / Interval + 1e-6);
	const int32 FutureCount = FMath::FloorToInt32(FMath::Max(static_cast<double>(PredictionSeconds), 0.0) / Interval + 1e-6);
	const double Oldest = Now - PastCount * Interval;
	while (History.Num() > 2 && History[1].Time <= Oldest)
	{
		History.RemoveAt(0);  // 最も古い標本の時刻より前を 1 つだけ残す（補間に使う）
	}

	Trajectory.Samples.Reset(PastCount + 1 + FutureCount);
	const auto AddSample = [this, &Facing](double Time, const FVector& Position)
	{
		FTransformTrajectorySample& Sample = Trajectory.Samples.AddDefaulted_GetRef();
		Sample.Facing = Facing;
		Sample.Position = Position;
		Sample.TimeInSeconds = static_cast<float>(Time);
	};
	for (int32 Index = PastCount; Index >= 1; --Index)
	{
		AddSample(-Index * Interval, HistoryAt(Now - Index * Interval));
	}
	AddSample(0.0, MeshNow);

	const UTennisMovementComponent* Movement = Character->GetTennisMovement();
	if (Movement == nullptr || FutureCount == 0)
	{
		return;
	}
	// 未来：移動のモデルの予測（シミュレーションの時間 = アニメの時間 × 時間の倍率）。カプセルの中心からメッシュへのずれは今のまま
	const double TimeScale = GetDefault<UTennisDebugSettings>()->GetTimeScale();
	TArray<double> SimTimes;
	SimTimes.Reserve(FutureCount);
	for (int32 Index = 1; Index <= FutureCount; ++Index)
	{
		SimTimes.Add(Index * Interval * TimeScale);
	}
	const TArray<FVector> Path = Movement->PredictWorldPath(SimTimes);
	const FVector MeshOffset = MeshNow - Character->GetActorLocation();
	for (int32 Index = 0; Index < Path.Num(); ++Index)
	{
		AddSample((Index + 1) * Interval, Path[Index] + MeshOffset);
	}
}

FVector UTennisAnimInstance::HistoryAt(double Time) const
{
	if (History.IsEmpty())
	{
		return FVector::ZeroVector;
	}
	if (Time <= History[0].Time)
	{
		return History[0].Position;
	}
	for (int32 Index = 1; Index < History.Num(); ++Index)
	{
		const FHistorySample& After = History[Index];
		if (Time <= After.Time)
		{
			const FHistorySample& Before = History[Index - 1];
			const double Span = After.Time - Before.Time;
			return Span > 0.0 ? FMath::Lerp(Before.Position, After.Position, (Time - Before.Time) / Span) : After.Position;
		}
	}
	return History.Last().Position;
}
