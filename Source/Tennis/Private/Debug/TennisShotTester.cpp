#include "Debug/TennisShotTester.h"

#include "Ball/TennisBall.h"
#include "Ball/TennisBallSimComponent.h"
#include "Components/InputComponent.h"
#include "Court/TennisCourt.h"
#include "Data/ShotTypeData.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Shot/ShotRequestMapping.h"
#include "Shot/TennisShotSolver.h"
#include "TennisSimLog.h"

namespace
{
	constexpr int32 ShotTypeMessageKey = 0x53484F54;  // 画面の表示を、同じ行で置き換える（今の球種）
	constexpr int32 LastShotMessageKey = 0x53484F55;  // 同じく（最後に打った球の結果）

	const TCHAR* FallbackName(EShotFallback Fallback)
	{
		switch (Fallback)
		{
		case EShotFallback::None:            return TEXT("None");
		case EShotFallback::RelaxConstraint: return TEXT("RelaxConstraint");
		case EShotFallback::SpeedClamped:    return TEXT("SpeedClamped");
		case EShotFallback::Nearest:         return TEXT("Nearest");
		}
		return TEXT("?");
	}
}

ATennisShotTester::ATennisShotTester()
{
	PrimaryActorTick.bCanEverTick = false;
}

FShotSolveResult ATennisShotTester::Shoot(UTennisBallSimComponent& Sim, const FShotTypeParams& Params, const FVector& HitPoint,
	const FVector& Target, double Charge)
{
	const FShotRequest Request = TennisShotRequestMapping::Make(
		Params, HitPoint, FVector(Target.X, Target.Y, 0.0), Sim.GetState().Time, Charge, ETennisHandedness::Right, 0.0);
	const FShotSolveResult Result = FTennisShotSolver(Sim.GetContext()).Solve(Request);
	Sim.Launch(Result.Launch);
	return Result;
}

TOptional<FVector> ATennisShotTester::GroundPointOnRay(const FVector& Start, const FVector& Direction)
{
	if (Start.Z <= 0.0 || Direction.Z >= -UE_DOUBLE_SMALL_NUMBER)
	{
		return {};
	}
	const FVector Point = Start + Direction * (-Start.Z / Direction.Z);
	return FVector(Point.X, Point.Y, 0.0);
}

void ATennisShotTester::BeginPlay()
{
	Super::BeginPlay();
	if (Ball == nullptr)
	{
		TActorIterator<ATennisBall> It(GetWorld());
		Ball = It ? *It : nullptr;
	}
	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	if (Ball == nullptr || Controller == nullptr || ShotTypes.IsEmpty())
	{
		UE_LOG(LogTennisShot, Warning, TEXT("[Tester] %s: needs a ball, a player and shot types (ball=%s, shot types=%d)"),
			*GetName(), *GetNameSafe(Ball), ShotTypes.Num());
		return;
	}

	// マウスで視点を回さず、カーソルでコートの地点を指す
	Controller->SetShowMouseCursor(true);
	Controller->SetIgnoreLookInput(true);
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	Controller->SetInputMode(InputMode);

	EnableInput(Controller);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ATennisShotTester::OnClick);
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ATennisShotTester::NextShotType);
	ShowCurrentShotType();
}

bool ATennisShotTester::ShootAt(FVector TargetLocal)
{
	return ShootWith(CurrentShotType(), TargetLocal);
}

bool ATennisShotTester::ShootWith(const UShotTypeData* Type, FVector TargetLocal)
{
	UTennisBallSimComponent* Sim = Ball != nullptr ? Ball->GetSim() : nullptr;
	if (Type == nullptr || Sim == nullptr)
	{
		UE_LOG(LogTennisShot, Warning, TEXT("[Tester] cannot shoot (ball=%s, shot type=%s)"), *GetNameSafe(Ball), *GetNameSafe(Type));
		return false;
	}

	const FShotSolveResult Result = Shoot(*Sim, Type->Params, HitPoint, TargetLocal, Charge);
	UE_LOG(LogTennisShot, Log, TEXT("[Tester] %s target=(%.2f, %.2f) solved=%d fallback=%s landing=(%.2f, %.2f) miss=%.3f m iterations=%d"),
		*Type->GetName(), TargetLocal.X, TargetLocal.Y, Result.bSolved, FallbackName(Result.Fallback),
		Result.PredictedLanding.X, Result.PredictedLanding.Y, Result.ResidualRange, Result.Iterations);
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(LastShotMessageKey, 600.0f, Result.bSolved ? FColor::Green : FColor::Orange,
			FString::Printf(TEXT("Last shot: %s  target (%.2f, %.2f) m  landing (%.2f, %.2f) m  miss %.3f m  fallback %s"),
				*Type->GetName(), TargetLocal.X, TargetLocal.Y, Result.PredictedLanding.X, Result.PredictedLanding.Y, Result.ResidualRange,
				FallbackName(Result.Fallback)));
	}
	return Result.bSolved;
}

void ATennisShotTester::NextShotType()
{
	if (!ShotTypes.IsEmpty())
	{
		ShotIndex = (ShotIndex + 1) % ShotTypes.Num();
		ShowCurrentShotType();
	}
}

void ATennisShotTester::OnClick()
{
	const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	const ATennisCourt* Court = Ball != nullptr ? Ball->GetSim()->GetCourt() : nullptr;
	FVector WorldStart;
	FVector WorldDirection;
	if (Controller == nullptr || Court == nullptr || !Controller->DeprojectMousePositionToWorld(WorldStart, WorldDirection))
	{
		return;
	}
	// 向きは 1 m 先の点で直す（コートの向きと位置を含めて、コート局所の m にする）
	const FVector Start = Court->WorldToLocalMeters(WorldStart);
	const FVector Ahead = Court->WorldToLocalMeters(WorldStart + WorldDirection * ATennisCourt::UnrealUnitsPerMeter);
	if (const TOptional<FVector> Target = GroundPointOnRay(Start, Ahead - Start))
	{
		ShootAt(Target.GetValue());
	}
}

const UShotTypeData* ATennisShotTester::CurrentShotType() const
{
	return ShotTypes.IsValidIndex(ShotIndex) ? ShotTypes[ShotIndex].Get() : nullptr;
}

void ATennisShotTester::ShowCurrentShotType() const
{
	const UShotTypeData* Type = CurrentShotType();
	UE_LOG(LogTennisShot, Log, TEXT("[Tester] shot type: %s"), *GetNameSafe(Type));
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(ShotTypeMessageKey, 600.0f, FColor::Yellow,
			FString::Printf(TEXT("Shot tester: %s  (click: shoot, Tab: next type)"), *GetNameSafe(Type)));
	}
}
