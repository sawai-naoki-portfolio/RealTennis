// T-INP-001（SPEC-04 §3.2）：ローカル選手に実際に登録される Context と、予約の取り消し。
#include "Components/ActorTestSpawner.h"
#include "Core/TennisInputSettings.h"
#include "CQTest.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Input/TennisPlayerController.h"
#include "Input/TennisShotInputComponent.h"
#include "Misc/ScopeExit.h"

TEST_CLASS(TennisInputPhase, "Tennis.Game.Input")
{
	FActorTestSpawner Spawner;

	// サーブ開始と離しで同じ Action を維持し、ロブのボタンをサーブに混ぜない。
	TEST_METHOD(ServeContexts_MapTheSameServeButtonsAndOnlyServingCanCancel)
	{
		const UTennisInputSettings* Settings = GetDefault<UTennisInputSettings>();
		const TArray<FName> Keys = { TEXT("J"), TEXT("L"), TEXT("K") };
		const TArray<FName> Pad = { TEXT("Gamepad_FaceButton_Bottom"), TEXT("Gamepad_FaceButton_Left"), TEXT("Gamepad_FaceButton_Right") };
		ASSERT_THAT(IsTrue(Settings->RallyShotButtons.Num() == 4));
		for (ETennisInputPhase Phase : { ETennisInputPhase::PreServe, ETennisInputPhase::Serving })
		{
			const FTennisPhaseContext* Entry = Settings->PhaseContexts.Find(Phase);
			ASSERT_THAT(IsTrue(Entry != nullptr));
			const UInputMappingContext* Context = Entry->Context.LoadSynchronous();
			ASSERT_THAT(IsTrue(Context != nullptr));
			auto HasKey = [Context](const UInputAction* Action, FName Key)
			{
				return Context->GetMappings().ContainsByPredicate([Action, Key](const FEnhancedActionKeyMapping& Mapping)
				{
					return Mapping.Action == Action && Mapping.Key.GetFName() == Key;
				});
			};
			for (int32 Index = 0; Index < Keys.Num(); ++Index)
			{
				const UInputAction* Shot = Settings->RallyShotButtons[Index].Action.LoadSynchronous();
				ASSERT_THAT(IsTrue(HasKey(Shot, Keys[Index])));
				ASSERT_THAT(IsTrue(HasKey(Shot, Pad[Index])));
			}
			const UInputAction* Lob = Settings->RallyShotButtons[3].Action.LoadSynchronous();
			ASSERT_THAT(IsFalse(HasKey(Lob, TEXT("I"))));
			const UInputAction* Cancel = LoadObject<UInputAction>(nullptr, TEXT("/Game/Tennis/Input/IA_TossCancel.IA_TossCancel"));
			ASSERT_THAT(IsTrue(Cancel != nullptr));
			ASSERT_THAT(AreEqual(Phase == ETennisInputPhase::Serving, HasKey(Cancel, TEXT("SpaceBar"))));
			ASSERT_THAT(AreEqual(Phase == ETennisInputPhase::Serving, HasKey(Cancel, TEXT("Gamepad_LeftShoulder"))));
		}
	}

	// トスで押した J は段階変更後も同じ押下。先に押していた Space は、トス取り消しの新規押下にしない。
	TEST_METHOD(HeldTossButton_ReleasesOnceAndHeldCancelDoesNotStart)
	{
		ATennisPlayerController& PC = Spawner.SpawnActor<ATennisPlayerController>();
		ULocalPlayer* Local = NewObject<ULocalPlayer>(GEngine);
		Local->PlayerAdded(nullptr, PLATFORMUSERID_NONE);
		ON_SCOPE_EXIT { Local->PlayerRemoved(); };
		PC.SetPlayer(Local);
		auto* Subsystem = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		auto* Input = Cast<UEnhancedInputComponent>(PC.InputComponent);
		auto* PlayerInput = Cast<UEnhancedPlayerInput>(PC.PlayerInput);
		ASSERT_THAT(IsTrue(Subsystem && Input && PlayerInput));
		const UInputAction* Shot = GetDefault<UTennisInputSettings>()->RallyShotButtons[0].Action.LoadSynchronous();
		const UInputAction* Cancel = LoadObject<UInputAction>(nullptr, TEXT("/Game/Tennis/Input/IA_TossCancel.IA_TossCancel"));
		ASSERT_THAT(IsTrue(Shot && Cancel));
		int32 Starts = 0, Releases = 0, Cancels = 0;
		Input->BindActionValueLambda(Shot, ETriggerEvent::Started, [&Starts](const FInputActionValue&) { ++Starts; });
		Input->BindActionValueLambda(Shot, ETriggerEvent::Completed, [&Releases](const FInputActionValue&) { ++Releases; });
		Input->BindActionValueLambda(Cancel, ETriggerEvent::Started, [&Cancels](const FInputActionValue&) { ++Cancels; });
		auto Frame = [&]()
		{
			TArray<UInputComponent*> Stack = { Input };
			PlayerInput->ProcessInputStack(Stack, 1.0f / 60.0f, false);
		};
		FModifyContextOptions Rebuild;
		Rebuild.bForceImmediately = true;
		ASSERT_THAT(IsTrue(PC.ApplyInputPhase(ETennisInputPhase::PreServe)));
		Subsystem->RequestRebuildControlMappings(Rebuild);
		PC.InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar, IE_Pressed, 1.0));
		PC.InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::J, IE_Pressed, 1.0));
		Frame();
		ASSERT_THAT(AreEqual(1, Starts));
		ASSERT_THAT(IsTrue(PC.ApplyInputPhase(ETennisInputPhase::Serving)));
		Subsystem->RequestRebuildControlMappings(Rebuild);
		Frame();
		ASSERT_THAT(AreEqual(0, Cancels));
		ASSERT_THAT(AreEqual(0, Releases));
		PC.InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::J, IE_Released, 0.0));
		Frame();
		Frame();
		ASSERT_THAT(AreEqual(1, Starts));
		ASSERT_THAT(AreEqual(1, Releases));
		PC.InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar, IE_Released, 0.0));
		Frame();
		PC.InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar, IE_Pressed, 1.0));
		Frame();
		ASSERT_THAT(AreEqual(1, Cancels));
	}

	TEST_METHOD(Switch_ReplacesOnlyPhaseContextAndCancelsRallyInput)
	{
		ATennisPlayerController& PC = Spawner.SpawnActor<ATennisPlayerController>();
		ULocalPlayer* Local = NewObject<ULocalPlayer>(GEngine);
		Local->PlayerAdded(nullptr, PLATFORMUSERID_NONE);
		ON_SCOPE_EXIT { Local->PlayerRemoved(); };
		PC.SetPlayer(Local);
		auto* Input = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		ASSERT_THAT(IsTrue(Input != nullptr));

		const UTennisInputSettings* Settings = GetDefault<UTennisInputSettings>();
		UInputMappingContext* Global = Settings->GlobalContext.Context.LoadSynchronous();
		UInputMappingContext* Rally = Settings->RallyContext.LoadSynchronous();
		ASSERT_THAT(IsTrue(Global != nullptr && Rally != nullptr));
		// SetPlayer の直後に Rally と Global が有効（試合開始後の参加でも同じ）。
		int32 Priority = -1;
		ASSERT_THAT(IsTrue(Input->HasMappingContext(Global, Priority)));
		ASSERT_THAT(AreEqual(0, Priority));
		ASSERT_THAT(IsTrue(Input->HasMappingContext(Rally, Priority)));
		ASSERT_THAT(AreEqual(10, Priority));

		UTennisShotInputComponent* Shot = PC.GetShotInput();
		Shot->OnShotPressed(0);
		ASSERT_THAT(IsTrue(Shot->GetState() == EShotInputState::Buffered));
		ASSERT_THAT(IsTrue(PC.ApplyInputPhase(ETennisInputPhase::Rally)));
		ASSERT_THAT(IsTrue(Shot->GetState() == EShotInputState::Buffered));
		UInputMappingContext* Prior = Rally;
		for (ETennisInputPhase Phase : { ETennisInputPhase::PreServe, ETennisInputPhase::Serving, ETennisInputPhase::Receive })
		{
			ASSERT_THAT(IsTrue(PC.ApplyInputPhase(Phase)));
			ASSERT_THAT(IsFalse(Input->HasMappingContext(Prior)));
			ASSERT_THAT(IsTrue(Input->HasMappingContext(Global)));
			Prior = Settings->PhaseContexts.FindChecked(Phase).Context.LoadSynchronous();
			ASSERT_THAT(IsTrue(Input->HasMappingContext(Prior, Priority)));
			ASSERT_THAT(AreEqual(Phase == ETennisInputPhase::Serving ? 20 : 10, Priority));
			Shot->OnShotPressed(0);  // 同じ球種 Action からラリーの予約を起動しない
			ASSERT_THAT(IsTrue(Shot->GetState() == EShotInputState::Idle));
		}
		ASSERT_THAT(IsTrue(PC.ApplyInputPhase(ETennisInputPhase::Menu)));
		ASSERT_THAT(IsFalse(Input->HasMappingContext(Prior)));
		ASSERT_THAT(IsTrue(Input->HasMappingContext(Global)));
		ASSERT_THAT(IsTrue(PC.ApplyInputPhase(ETennisInputPhase::Rally)));
		Shot->OnShotPressed(0);
		ASSERT_THAT(IsTrue(Shot->GetState() == EShotInputState::Buffered));
	}
};
