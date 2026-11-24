#include "Player/TennisMovementComponent.h"

#include "Core/TennisDebugSettings.h"
#include "Court/TennisCourt.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "TennisGameLog.h"

UTennisMovementComponent::UTennisMovementComponent()
{
	RotationRate = FRotator(0.0, 720.0, 0.0);  // DD-03 §2.2
	bOrientRotationToMovement = false;          // 選手は基本的にネットの方向を向く
	// CMC のほかの処理（アニメの速さなど）が見る上限。水平の速度そのものは TennisMovement が決める
	MaxWalkSpeed = static_cast<float>(ATennisCourt::LocalMetersToWorldVector(FTransform::Identity,
		FVector(Params.MaxSpeed * Params.DashSpeedFactor, 0.0, 0.0)).X);
}

void UTennisMovementComponent::StartBoost()
{
	TennisMovement::StartBoost(Stepper.State, Params);
}

FVector UTennisMovementComponent::AdvanceWorldVelocity(const FTransform& CourtTransform, const FVector& WorldLocation, const FVector& WorldVelocity,
	const FVector& WorldInput, double SideSign, FMovementStepper& Stepper, const FMovementParams& Params, double FrameTime, double TimeScale,
	const FMovementModifiers& Modifiers)
{
	const FVector Local = ATennisCourt::WorldToLocalMeters(CourtTransform, WorldLocation);
	const FVector LocalInput = CourtTransform.InverseTransformVectorNoScale(WorldInput);  // 向きだけ（長さはそのまま）
	const FVector2D Planar = TennisMovement::AdvanceFrame(Stepper, FVector2D(Local.X, Local.Y), FVector2D(LocalInput.X, LocalInput.Y), SideSign,
		Modifiers, Params, FrameTime, TimeScale);
	const FVector Horizontal = ATennisCourt::LocalMetersToWorldVector(CourtTransform, FVector(Planar, 0.0));
	return FVector(Horizontal.X, Horizontal.Y, WorldVelocity.Z);
}

FVector UTennisMovementComponent::CurrentInput() const
{
	return Acceleration.GetSafeNormal() * GetAnalogInputModifier();  // CMC が受け取った移動の入力の向き × スティックの倒し具合
}

TArray<FVector> UTennisMovementComponent::PredictWorldPath(TConstArrayView<double> Times) const
{
	TArray<FVector> Out;
	if (Court == nullptr || UpdatedComponent == nullptr)
	{
		return Out;
	}
	const FTransform& CourtTransform = Court->GetActorTransform();
	const FVector Local = ATennisCourt::WorldToLocalMeters(CourtTransform, UpdatedComponent->GetComponentLocation());
	const FVector LocalInput = CourtTransform.InverseTransformVectorNoScale(CurrentInput());
	TArray<double> FromLastStep;  // PredictPath の時刻 0 は最後のステップ。外の位置は、まだステップにしていない時間の分だけ先にいる
	FromLastStep.Reserve(Times.Num());
	for (const double Time : Times)
	{
		FromLastStep.Add(Time + Stepper.Clock.Remainder());
	}
	const TArray<FVector2D> Path = TennisMovement::PredictPath(Stepper.State, FVector2D(Local.X, Local.Y) - Stepper.Lead, Stepper.AutoMove,
		FVector2D(LocalInput.X, LocalInput.Y), SideSign, Modifiers, Params, FromLastStep);
	Out.Reserve(Path.Num());
	for (const FVector2D& Point : Path)
	{
		Out.Add(ATennisCourt::LocalMetersToWorld(CourtTransform, FVector(Point, Local.Z)));
	}
	return Out;
}

void UTennisMovementComponent::SetAutoMoveTarget(const FVector2D& CourtTarget, double TimeLeft, double SettleTime)
{
	Stepper.AutoMove = FAutoMoveTarget{ CourtTarget, TimeLeft, SettleTime };
	UE_LOG(LogTennisMove, Log, TEXT("[Move] %s: auto move target=%.2f,%.2f time_left=%.3f settle=%.3f"), *GetNameSafe(GetOwner()),
		CourtTarget.X, CourtTarget.Y, TimeLeft, SettleTime);
}

void UTennisMovementComponent::ClearAutoMoveTarget()
{
	Stepper.AutoMove.Reset();
}

void UTennisMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	if (Court == nullptr && GetWorld() != nullptr)
	{
		TActorIterator<ATennisCourt> It(GetWorld());
		Court = It ? *It : nullptr;
	}
	if (Court == nullptr)
	{
		UE_LOG(LogTennisMove, Warning, TEXT("[Move] %s: no ATennisCourt in the level; using the default character movement"), *GetNameSafe(GetOwner()));
		return;
	}
	SideSign = Court->WorldToLocalMeters(GetActorLocation()).X < 0.0 ? -1.0 : 1.0;
}

void UTennisMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (Court == nullptr || HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity())
	{
		Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
		return;
	}
	Velocity = AdvanceWorldVelocity(Court->GetActorTransform(), UpdatedComponent->GetComponentLocation(), Velocity, CurrentInput(), SideSign, Stepper, Params,
		DeltaTime, GetDefault<UTennisDebugSettings>()->GetTimeScale(), Modifiers);  // ボールと同じ時間の倍率（tennis.TimeScale）
}
