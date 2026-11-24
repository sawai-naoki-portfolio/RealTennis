#include "Input/TennisPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Camera/TennisPlayerCameraManager.h"
#include "Core/TennisInputSettings.h"
#include "Court/TennisCourt.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Input/TennisShotInputComponent.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisMovementComponent.h"
#include "TennisGameLog.h"

ATennisPlayerController::ATennisPlayerController()
{
	ShotInput = CreateDefaultSubobject<UTennisShotInputComponent>(TEXT("ShotInput"));
	PlayerCameraManagerClass = ATennisPlayerCameraManager::StaticClass();  // 中継カメラ（DD-07 §1）
}

FVector ATennisPlayerController::StickToWorldDirection(const FVector2D& Stick, double ForwardYawDegrees)
{
	if (!FMath::IsFinite(Stick.X) || !FMath::IsFinite(Stick.Y) || !FMath::IsFinite(ForwardYawDegrees))
	{
		return FVector::ZeroVector;
	}
	const FVector2D Clamped = Stick.SizeSquared() > 1.0 ? Stick.GetSafeNormal() : Stick;
	const double Yaw = FMath::DegreesToRadians(ForwardYawDegrees);
	const FVector Forward(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);  // 上
	const FVector Right(-FMath::Sin(Yaw), FMath::Cos(Yaw), 0.0);   // 右（UE は左手系：前 X・右 Y）
	return Forward * Clamped.Y + Right * Clamped.X;
}

double ATennisPlayerController::NetwardYawDegrees(double CourtYawDegrees, double Side)
{
	return FRotator::NormalizeAxis(CourtYawDegrees + (Side > 0.0 ? 180.0 : 0.0));
}

void ATennisPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	if (GetLocalPlayer() != nullptr)
	{
		ApplyInputPhase(ETennisInputPhase::Rally);
	}
}

bool ATennisPlayerController::ApplyInputPhase(ETennisInputPhase Phase)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem == nullptr)
	{
		return false;
	}
	if (AppliedPhase.IsSet() && AppliedPhase.GetValue() == Phase)
	{
		return true;
	}
	const UTennisInputSettings* Settings = GetDefault<UTennisInputSettings>();
	const FTennisPhaseContext* Entry = Settings->PhaseContexts.Find(Phase);
	UInputMappingContext* Next = nullptr;
	int32 Priority = 0;
	if (Phase == ETennisInputPhase::Rally)
	{
		Next = Settings->RallyContext.LoadSynchronous();
		Priority = Settings->RallyContextPriority;
	}
	else if (Phase != ETennisInputPhase::Menu && Entry != nullptr)
	{
		Next = Entry->Context.LoadSynchronous();
		Priority = Entry->Priority;
	}
	if (Next == nullptr && Phase != ETennisInputPhase::Menu)
	{
		UE_LOG(LogTennisInput, Warning, TEXT("[Input] MissingContext phase=%d"), static_cast<int32>(Phase));
		return false;
	}
	if (ActiveGlobalContext == nullptr)
	{
		ActiveGlobalContext = Settings->GlobalContext.Context.LoadSynchronous();
		if (ActiveGlobalContext == nullptr)
		{
			UE_LOG(LogTennisInput, Warning, TEXT("[Input] MissingGlobalContext"));
			return false;
		}
		Subsystem->AddMappingContext(ActiveGlobalContext, Settings->GlobalContext.Priority);
	}
	// 同一キー・同一 Action のトス入力は再構築時に継続する。新しく割り当てたキャンセルキーは離すまで無視する。
	if (ActiveContext != nullptr)
	{
		Subsystem->RemoveMappingContext(ActiveContext);
	}
	ShotInput->SetRallyInputEnabled(Phase == ETennisInputPhase::Rally);
	ActiveContext = Next;
	AppliedPhase = Phase;
	if (Next != nullptr)
	{
		Subsystem->AddMappingContext(Next, Priority);
	}
	return true;
}

void ATennisPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	ULocalPlayer* Local = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Local);
	if (Subsystem != nullptr && Local->PlayerController == this)
	{
		if (ActiveContext != nullptr)
		{
			Subsystem->RemoveMappingContext(ActiveContext);
		}
		if (ActiveGlobalContext != nullptr)
		{
			Subsystem->RemoveMappingContext(ActiveGlobalContext);
		}
	}
	Super::EndPlay(Reason);
}

void ATennisPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (Input == nullptr)
	{
		UE_LOG(LogTennisInput, Warning, TEXT("[Input] %s: the input component is not Enhanced Input (%s)"), *GetNameSafe(this), *GetNameSafe(InputComponent));
		return;
	}
	if (UInputAction* Move = GetDefault<UTennisInputSettings>()->MoveAction.LoadSynchronous())
	{
		Input->BindAction(Move, ETriggerEvent::Triggered, this, &ATennisPlayerController::OnMove);
	}
	else
	{
		UE_LOG(LogTennisInput, Warning, TEXT("[Input] %s: no move action. Set it in Project Settings > Tennis Input"), *GetName());
	}
	ShotInput->BindInput(*Input);
}

void ATennisPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ShotInput->Cancel();  // 前の選手への予約を持ち越さない
	ShotInput->BindCharacter(Cast<ATennisCharacter>(InPawn));
}

void ATennisPlayerController::OnUnPossess()
{
	ShotInput->Cancel();  // 離れる前に、今の選手の予約を取り消す
	ShotInput->BindCharacter(nullptr);
	Super::OnUnPossess();
}

void ATennisPlayerController::OnMove(const FInputActionValue& Value)
{
	if (AppliedPhase == ETennisInputPhase::Serving || AppliedPhase == ETennisInputPhase::Menu)
	{
		return;  // トス中のスティックはサーブの狙いに使う
	}
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn == nullptr)
	{
		return;
	}
	// 上＝ネットの方向（DD-03 §2.2）。コートが見つからないときだけカメラの向き
	double ForwardYaw = PlayerCameraManager != nullptr ? PlayerCameraManager->GetCameraRotation().Yaw : GetControlRotation().Yaw;
	const ATennisCharacter* TennisCharacter = Cast<ATennisCharacter>(ControlledPawn);
	const UTennisMovementComponent* Movement = TennisCharacter != nullptr ? TennisCharacter->GetTennisMovement() : nullptr;
	if (const ATennisCourt* Court = Movement != nullptr ? Movement->GetCourt() : nullptr)
	{
		ForwardYaw = NetwardYawDegrees(Court->GetActorRotation().Yaw, Movement->GetSideSign());
	}
	ControlledPawn->AddMovementInput(StickToWorldDirection(Value.Get<FVector2D>(), ForwardYaw));
}
