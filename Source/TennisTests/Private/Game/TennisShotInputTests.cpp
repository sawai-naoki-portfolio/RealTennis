// W-3.2：球種のボタンの設定と、押した意図が選手に届くこと（T-INP-002、操作仕様 §2・§3.1、ST-05）。
#include "Components/ActorTestSpawner.h"
#include "Core/TennisInputSettings.h"
#include "CQTest.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Input/TennisPlayerController.h"
#include "Input/TennisShotInputComponent.h"
#include "Player/TennisCharacter.h"

namespace
{
	/** Context の中で Action に割り当てたキーの名前 */
	TSet<FName> KeysOf(const UInputMappingContext& Context, const UInputAction* Action)
	{
		TSet<FName> Keys;
		for (const FEnhancedActionKeyMapping& Mapping : Context.GetMappings())
		{
			if (Mapping.Action == Action)
			{
				Keys.Add(Mapping.Key.GetFName());
			}
		}
		return Keys;
	}
}

TEST_CLASS(TennisShotInput, "Tennis.Game.Input.ShotInput")
{
	FActorTestSpawner Spawner;

	// ボタンの並びと球種（操作仕様 §3.1）、IMC_Rally のキーとゲームパッドのボタン（§2.1・§2.2）
	TEST_METHOD(Settings_MapShotButtonsInTheRallyContext)
	{
		const UTennisInputSettings* Settings = GetDefault<UTennisInputSettings>();
		const UInputMappingContext* Rally = Settings->RallyContext.LoadSynchronous();
		ASSERT_THAT(IsTrue(Rally != nullptr));

		struct FExpected { ETennisShotType Type; ETennisShotType TechniqueType; const TCHAR* Key; const TCHAR* Pad; };
		const FExpected Expected[] = {
			{ ETennisShotType::Topspin, ETennisShotType::Topspin,  TEXT("J"), TEXT("Gamepad_FaceButton_Bottom") },
			{ ETennisShotType::Flat,    ETennisShotType::Flat,     TEXT("L"), TEXT("Gamepad_FaceButton_Left") },
			{ ETennisShotType::Slice,   ETennisShotType::Drop,     TEXT("K"), TEXT("Gamepad_FaceButton_Right") },
			{ ETennisShotType::Lob,     ETennisShotType::SliceLob, TEXT("I"), TEXT("Gamepad_FaceButton_Top") },
		};
		ASSERT_THAT(AreEqual(static_cast<int32>(UE_ARRAY_COUNT(Expected)), Settings->RallyShotButtons.Num()));
		for (int32 Button = 0; Button < Settings->RallyShotButtons.Num(); ++Button)
		{
			const FTennisShotButton& Entry = Settings->RallyShotButtons[Button];
			const UInputAction* Action = Entry.Action.LoadSynchronous();
			ASSERT_THAT(IsTrue(Action != nullptr && Action->ValueType == EInputActionValueType::Boolean, Expected[Button].Key));
			ASSERT_THAT(IsTrue(Entry.Type == Expected[Button].Type && Entry.TechniqueType == Expected[Button].TechniqueType, Expected[Button].Key));
			const TSet<FName> Keys = KeysOf(*Rally, Action);
			ASSERT_THAT(IsTrue(Keys.Contains(FName(Expected[Button].Key)) && Keys.Contains(FName(Expected[Button].Pad)), Expected[Button].Key));
		}

		const UInputAction* Technique = Settings->TechniqueAction.LoadSynchronous();
		ASSERT_THAT(IsTrue(Technique != nullptr));
		const TSet<FName> TechniqueKeys = KeysOf(*Rally, Technique);
		ASSERT_THAT(IsTrue(TechniqueKeys.Contains(FName(TEXT("O"))) && TechniqueKeys.Contains(FName(TEXT("Gamepad_RightShoulder")))));
	}

	// T-INP-002：押した意図は、同じフレームのうちに選手に届く。修飾で球種が変わり、候補の前に離すと取り消される
	TEST_METHOD(Press_ReachesTheCharacterInTheSameFrame)
	{
		ATennisCharacter& Character = Spawner.SpawnActor<ATennisCharacter>();
		ATennisPlayerController& Controller = Spawner.SpawnActor<ATennisPlayerController>();
		Controller.Possess(&Character);
		UEnhancedInputComponent* Input = NewObject<UEnhancedInputComponent>(&Controller);
		UTennisShotInputComponent* ShotInput = Controller.GetShotInput();
		ShotInput->BindInput(*Input);

		const uint64 Frame = GFrameCounter;
		ShotInput->OnShotPressed(2);  // スライスのボタン
		const FShotIntent* Intent = Character.GetShotIntent();
		ASSERT_THAT(IsTrue(Intent != nullptr && Intent->Type == ETennisShotType::Slice));
		ASSERT_THAT(IsTrue(GFrameCounter == Frame));
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Buffered));  // 打点の候補（W-3.3）がまだない

		ShotInput->OnTechnique(true);
		ASSERT_THAT(IsTrue(Character.GetShotIntent() != nullptr && Character.GetShotIntent()->Type == ETennisShotType::Drop));

		ShotInput->OnShotReleased(2);
		ASSERT_THAT(IsTrue(Character.GetShotIntent() == nullptr));
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Idle));
	}

	// 選手から離れたら、その選手への予約を取り消す
	TEST_METHOD(UnPossess_CancelsTheReservation)
	{
		ATennisCharacter& Character = Spawner.SpawnActor<ATennisCharacter>();
		ATennisPlayerController& Controller = Spawner.SpawnActor<ATennisPlayerController>();
		Controller.Possess(&Character);
		UEnhancedInputComponent* Input = NewObject<UEnhancedInputComponent>(&Controller);
		Controller.GetShotInput()->BindInput(*Input);

		Controller.GetShotInput()->OnShotPressed(0);
		ASSERT_THAT(IsTrue(Character.GetShotIntent() != nullptr));
		Controller.UnPossess();
		ASSERT_THAT(IsTrue(Character.GetShotIntent() == nullptr));
		ASSERT_THAT(IsTrue(Controller.GetShotInput()->GetState() == EShotInputState::Idle));
	}
};
