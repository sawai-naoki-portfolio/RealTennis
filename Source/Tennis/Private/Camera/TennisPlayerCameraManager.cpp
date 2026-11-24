#include "Camera/TennisPlayerCameraManager.h"

#include "Ball/TennisBall.h"
#include "Camera/TennisCameraMode_Broadcast.h"
#include "Court/TennisCourt.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisMovementComponent.h"
#include "TennisGameLog.h"

ATennisPlayerCameraManager::ATennisPlayerCameraManager()
{
	Mode = CreateDefaultSubobject<UTennisCameraMode_Broadcast>(TEXT("Broadcast"));
}

void ATennisPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	FTennisCameraContext Context;
	if (Mode == nullptr || !MakeContext(Context))
	{
		if (!bWarnedNoContext)
		{
			UE_LOG(LogTennisCamera, Warning, TEXT("[Camera] %s: no tennis player or court to follow (using the default view target)"), *GetName());
			bWarnedNoContext = true;
		}
		Super::UpdateViewTarget(OutVT, DeltaTime);
		return;
	}
	bWarnedNoContext = false;

	const APawn* Pawn = PCOwner->GetPawn();
	if (EvaluatedPawn.Get() != Pawn)  // 選手が替わった：前の選手の位置から飛んでこないように
	{
		Mode->Reset();
		EvaluatedPawn = Pawn;
	}
	OutVT.POV = Mode->Evaluate(Context, DeltaTime);
}

bool ATennisPlayerCameraManager::MakeContext(FTennisCameraContext& Out)
{
	const ATennisCharacter* Character = PCOwner != nullptr ? Cast<ATennisCharacter>(PCOwner->GetPawn()) : nullptr;
	const UTennisMovementComponent* Movement = Character != nullptr ? Character->GetTennisMovement() : nullptr;
	const ATennisCourt* Court = Movement != nullptr ? Movement->GetCourt() : nullptr;
	if (Court == nullptr)
	{
		return false;
	}
	if (!Ball.IsValid())
	{
		TActorIterator<ATennisBall> It(GetWorld());
		Ball = It ? *It : nullptr;
	}

	Out.CourtTransform = Court->GetActorTransform();
	Out.Player = Court->WorldToLocalMeters(Character->GetActorLocation());
	Out.Ball = Ball.IsValid() ? Court->WorldToLocalMeters(Ball->GetActorLocation()) : Out.Player;  // 見た目の位置（補間）
	Out.Side = Movement->GetSideSign();

	const ULocalPlayer* LocalPlayer = PCOwner->GetLocalPlayer();
	FVector2D ViewportSize = FVector2D::ZeroVector;
	if (LocalPlayer != nullptr && LocalPlayer->ViewportClient != nullptr)
	{
		LocalPlayer->ViewportClient->GetViewportSize(ViewportSize);
	}
	if (ViewportSize.X > 0.0 && ViewportSize.Y > 0.0)
	{
		Out.ViewAspect = ViewportSize.X / ViewportSize.Y;
	}
	return true;
}
