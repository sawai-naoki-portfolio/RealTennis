#include "Input/TennisShotInputComponent.h"

#include "Core/TennisInputSettings.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Controller.h"
#include "InputAction.h"
#include "Player/TennisCharacter.h"
#include "TennisGameLog.h"

UTennisShotInputComponent::UTennisShotInputComponent()
{
	PrimaryComponentTick.bCanEverTick = true;  // バッファの時間切れを見る（時刻で判定し、DeltaTime は使わない）
}

void UTennisShotInputComponent::BindInput(UEnhancedInputComponent& Input)
{
	const UTennisInputSettings* Settings = GetDefault<UTennisInputSettings>();
	Machine = FTennisShotInputMachine(Settings->InputBufferSeconds);
	ButtonTypes.Reset();
	for (int32 Button = 0; Button < Settings->RallyShotButtons.Num(); ++Button)
	{
		const FTennisShotButton& Entry = Settings->RallyShotButtons[Button];
		ButtonTypes.Add({ Entry.Type, Entry.TechniqueType });
		const UInputAction* Action = Entry.Action.LoadSynchronous();
		if (Action == nullptr)
		{
			UE_LOG(LogTennisInput, Warning, TEXT("[ShotInput] button %d has no action (%s). Set it in Project Settings > Tennis Input"),
				Button, *Entry.Action.ToString());
			continue;
		}
		Input.BindAction(Action, ETriggerEvent::Started, this, &UTennisShotInputComponent::OnShotPressed, Button);
		Input.BindAction(Action, ETriggerEvent::Completed, this, &UTennisShotInputComponent::OnShotReleased, Button);
	}
	if (const UInputAction* Technique = Settings->TechniqueAction.LoadSynchronous())
	{
		Input.BindAction(Technique, ETriggerEvent::Started, this, &UTennisShotInputComponent::OnTechnique, true);
		Input.BindAction(Technique, ETriggerEvent::Completed, this, &UTennisShotInputComponent::OnTechnique, false);
	}
	else
	{
		UE_LOG(LogTennisInput, Warning, TEXT("[ShotInput] no technique action (%s)"), *Settings->TechniqueAction.ToString());
	}
}

void UTennisShotInputComponent::SetRallyInputEnabled(bool bEnabled)
{
	Cancel();
	Machine.SetTechnique(false);
	bRallyInputEnabled = bEnabled;
}

void UTennisShotInputComponent::OnShotPressed(int32 Button)
{
	if (!bRallyInputEnabled)
	{
		return;
	}
	if (!ButtonTypes.IsValidIndex(Button))
	{
		UE_LOG(LogTennisInput, Warning, TEXT("[ShotInput] unknown button %d"), Button);
		return;
	}
	Publish(Machine.Press(Button, ButtonTypes[Button], GetNow()), TEXT("pressed"));
}

void UTennisShotInputComponent::OnShotReleased(int32 Button)
{
	if (!bRallyInputEnabled)
	{
		return;
	}
	Publish(Machine.Release(Button, GetNow()), TEXT("released"));
}

void UTennisShotInputComponent::OnTechnique(bool bHeld)
{
	if (!bRallyInputEnabled)
	{
		return;
	}
	Publish(Machine.SetTechnique(bHeld), bHeld ? TEXT("technique on") : TEXT("technique off"));
}

void UTennisShotInputComponent::Cancel()
{
	Publish(Machine.Cancel(), TEXT("canceled"));
}

void UTennisShotInputComponent::BindCharacter(ATennisCharacter* Character)
{
	if (ATennisCharacter* Previous = BoundCharacter.Get())
	{
		Previous->OnHitCandidateFound.Remove(CandidateHandle);
		Previous->OnStrokeEnded.Remove(StrokeEndedHandle);
		Previous->OnStrokeImpact.Remove(ImpactHandle);
	}
	BoundCharacter = Character;
	CandidateHandle.Reset();
	StrokeEndedHandle.Reset();
	ImpactHandle.Reset();
	if (Character != nullptr)
	{
		CandidateHandle = Character->OnHitCandidateFound.AddUObject(this, &UTennisShotInputComponent::OnCandidateFound);
		StrokeEndedHandle = Character->OnStrokeEnded.AddUObject(this, &UTennisShotInputComponent::OnStrokeEnded);
		ImpactHandle = Character->OnStrokeImpact.AddUObject(this, &UTennisShotInputComponent::OnStrokeImpact);
	}
}

void UTennisShotInputComponent::OnCandidateFound()
{
	Publish(Machine.CandidateFound(GetNow()), TEXT("candidate found"));
}

void UTennisShotInputComponent::OnStrokeEnded()
{
	Publish(Machine.StrokeEnded(), TEXT("stroke ended"));
}

void UTennisShotInputComponent::OnStrokeImpact()
{
	Publish(Machine.Impact(), TEXT("impact"));  // 離した → 確定。押したまま（溜め中）なら自動で離した扱いで確定（操作仕様 §4.1）
}

void UTennisShotInputComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Publish(Machine.Update(GetNow()), TEXT("timed out"));
}

void UTennisShotInputComponent::Publish(bool bChanged, const TCHAR* What)
{
	if (!bChanged)
	{
		return;
	}
	const EShotInputState State = Machine.GetState();
	UE_LOG(LogTennisInput, Log, TEXT("[ShotInput] %s -> %s (%s)"), What, LexToString(State), *UEnum::GetValueAsString(Machine.GetIntent().Type));
	ATennisCharacter* Character = GetCharacter();
	if (Character == nullptr)
	{
		return;  // 操作する選手がいない（観戦など）。入力の状態だけを進める
	}
	if (State == EShotInputState::Idle)
	{
		Character->CancelShotIntent();
	}
	else
	{
		Character->SubmitShotIntent(Machine.GetIntent());
	}
}

ATennisCharacter* UTennisShotInputComponent::GetCharacter() const
{
	const AController* Controller = Cast<AController>(GetOwner());
	return Controller != nullptr ? Cast<ATennisCharacter>(Controller->GetPawn()) : nullptr;
}

double UTennisShotInputComponent::GetNow() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetTimeSeconds() : 0.0;
}
