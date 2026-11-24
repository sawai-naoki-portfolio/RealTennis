// W-3.2（移動の入力）：コートに対する移動の向き（DD-03 §2.2）、ゲームモードの既定、操作のアセットの参照（操作仕様 §2・§3）。
#include "CQTest.h"
#include "Core/TennisGameMode.h"
#include "Core/TennisInputSettings.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Input/TennisPlayerController.h"
#include "Player/TennisCharacter.h"

TEST_CLASS(TennisInput, "Tennis.Game.Input")
{
	// 上は前の向き（Yaw）、右はその右。斜めの入力も長さ 1。数でない入力は 0
	TEST_METHOD(StickToWorldDirection_UsesTheForwardYaw)
	{
		ASSERT_THAT(IsTrue(ATennisPlayerController::StickToWorldDirection(FVector2D(0.0, 1.0), 0.0).Equals(FVector(1.0, 0.0, 0.0), 1e-12)));
		ASSERT_THAT(IsTrue(ATennisPlayerController::StickToWorldDirection(FVector2D(1.0, 0.0), 0.0).Equals(FVector(0.0, 1.0, 0.0), 1e-12)));
		ASSERT_THAT(IsTrue(ATennisPlayerController::StickToWorldDirection(FVector2D(0.0, 1.0), 90.0).Equals(FVector(0.0, 1.0, 0.0), 1e-12)));
		ASSERT_THAT(IsNear(1.0, ATennisPlayerController::StickToWorldDirection(FVector2D(1.0, 1.0), 30.0).Size(), 1e-12));
		ASSERT_THAT(IsNear(0.5, ATennisPlayerController::StickToWorldDirection(FVector2D(0.0, 0.5), 0.0).Size(), 1e-12));
		ASSERT_THAT(IsTrue(ATennisPlayerController::StickToWorldDirection(
			FVector2D(std::numeric_limits<double>::quiet_NaN(), 1.0), 0.0).IsZero()));
	}

	// 上はネットの方向（DD-03 §2.2）：X < 0 の側はコートの +X、X > 0 の側は −X。回したコートでも同じ。右は自分から見たコートの右
	TEST_METHOD(NetwardYaw_FollowsTheCourtAndTheSide)
	{
		ASSERT_THAT(IsNear(0.0, ATennisPlayerController::NetwardYawDegrees(0.0, -1.0), 1e-12));
		ASSERT_THAT(IsNear(180.0, FMath::Abs(ATennisPlayerController::NetwardYawDegrees(0.0, 1.0)), 1e-12));
		ASSERT_THAT(IsNear(90.0, ATennisPlayerController::NetwardYawDegrees(90.0, -1.0), 1e-12));
		ASSERT_THAT(IsNear(-90.0, ATennisPlayerController::NetwardYawDegrees(90.0, 1.0), 1e-12));
		// X > 0 の側で右（D）を押すと、自分から見た右＝コートの −Y へ動く
		const FVector Right = ATennisPlayerController::StickToWorldDirection(
			FVector2D(1.0, 0.0), ATennisPlayerController::NetwardYawDegrees(0.0, 1.0));
		ASSERT_THAT(IsTrue(Right.Equals(FVector(0.0, -1.0, 0.0), 1e-12)));
	}

	// ゲームモードは、選手のキャラクターと選手のコントローラーを使う
	TEST_METHOD(GameMode_UsesTennisPawnAndController)
	{
		const ATennisGameMode* GameMode = GetDefault<ATennisGameMode>();
		ASSERT_THAT(IsTrue(GameMode->DefaultPawnClass == ATennisCharacter::StaticClass()));
		ASSERT_THAT(IsTrue(GameMode->PlayerControllerClass == ATennisPlayerController::StaticClass()));
	}

	// 設定（Config/DefaultGame.ini）は IA_Move（Axis2D）と IMC_Rally を指し、IMC_Rally は WASD と左スティックを移動に割り当てる
	TEST_METHOD(Settings_PointToMoveActionMappedToKeysAndStick)
	{
		const UTennisInputSettings* Settings = GetDefault<UTennisInputSettings>();
		const UInputAction* Move = Settings->MoveAction.LoadSynchronous();
		const UInputMappingContext* Rally = Settings->RallyContext.LoadSynchronous();
		ASSERT_THAT(IsTrue(Move != nullptr && Rally != nullptr));
		ASSERT_THAT(IsTrue(Move->ValueType == EInputActionValueType::Axis2D));

		TSet<FName> MoveKeys;
		for (const FEnhancedActionKeyMapping& Mapping : Rally->GetMappings())
		{
			if (Mapping.Action == Move)
			{
				MoveKeys.Add(Mapping.Key.GetFName());
			}
		}
		for (const TCHAR* Key : { TEXT("W"), TEXT("A"), TEXT("S"), TEXT("D"), TEXT("Gamepad_Left2D") })
		{
			ASSERT_THAT(IsTrue(MoveKeys.Contains(FName(Key)), Key));
		}
	}
};
