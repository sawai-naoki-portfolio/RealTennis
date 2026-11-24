// UTennisDebugSettings：値の置き場所が 1 か所であること（コマンドと、MCP と同じリフレクションの経路が同じ値を見る）、
// 不正な入力を受け付けないこと、範囲外の値を丸めることを確かめる（ARCH-04 §3.1）。
#include "CQTest.h"
#include "Core/TennisDebugSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/StringOutputDevice.h"
#include "UObject/UnrealType.h"

namespace
{
	/** コマンドを実行し、出力を返す */
	FString Run(const TCHAR* Command, const TArray<FString>& Args = {})
	{
		IConsoleObject* Object = IConsoleManager::Get().FindConsoleObject(Command);
		check(Object && Object->AsCommand());
		FStringOutputDevice Out;
		Object->AsCommand()->Execute(Args, nullptr, Out);
		return Out;
	}

	/** Unreal MCP の ConfigSettingsToolset と同じく、リフレクションでプロパティに値を書く */
	void SetByReflection(FName PropertyName, const TCHAR* Value)
	{
		UTennisDebugSettings* Settings = GetMutableDefault<UTennisDebugSettings>();
		FProperty* Property = FindFProperty<FProperty>(UTennisDebugSettings::StaticClass(), PropertyName);
		check(Property);
		Property->ImportText_InContainer(Value, Settings, Settings, PPF_None);
	}
}

TEST_CLASS(TennisDebugSettings, "Tennis.Game.Debug")
{
	const UTennisDebugSettings* Settings = nullptr;
	int32 SavedTrajectory = 0;
	float SavedTimeScale = 1.0f;

	BEFORE_EACH()
	{
		Settings = GetDefault<UTennisDebugSettings>();
		SavedTrajectory = Settings->GetTrajectoryMode();
		SavedTimeScale = Settings->GetTimeScale();
	}

	AFTER_EACH()
	{
		Run(UTennisDebugSettings::TrajectoryCommandName, { LexToString(SavedTrajectory) });
		Run(UTennisDebugSettings::TimeScaleCommandName, { LexToString(SavedTimeScale) });
	}

	TEST_METHOD(IsPerUserConfig_NotCommittedDefaultConfig)
	{
		// DefaultConfig だと、MCP の ConfigSettingsToolset が追跡対象の Config/DefaultGame.ini に書いてしまう
		ASSERT_THAT(IsFalse(UTennisDebugSettings::StaticClass()->HasAnyClassFlags(CLASS_DefaultConfig)));
		ASSERT_THAT(IsTrue(UTennisDebugSettings::StaticClass()->HasAnyClassFlags(CLASS_Config)));
	}

	TEST_METHOD(Commands_AreRegistered)
	{
		for (const TCHAR* Name : { UTennisDebugSettings::TrajectoryCommandName, UTennisDebugSettings::TimeScaleCommandName })
		{
			IConsoleObject* Object = IConsoleManager::Get().FindConsoleObject(Name);
			ASSERT_THAT(IsTrue(Object != nullptr && Object->AsCommand() != nullptr));
		}
	}

	TEST_METHOD(Command_ChangesSetting)
	{
		Run(UTennisDebugSettings::TrajectoryCommandName, { TEXT("2") });
		ASSERT_THAT(AreEqual(2, Settings->GetTrajectoryMode()));
	}

	TEST_METHOD(ValueWrittenByReflection_IsReportedByCommand)
	{
		SetByReflection(TEXT("DebugTrajectory"), TEXT("1"));
		ASSERT_THAT(AreEqual(1, Settings->GetTrajectoryMode()));
		ASSERT_THAT(IsTrue(Run(UTennisDebugSettings::TrajectoryCommandName).Contains(TEXT("= 1"))));
	}

	TEST_METHOD(InvalidInput_IsRejected)
	{
		Run(UTennisDebugSettings::TrajectoryCommandName, { TEXT("1") });
		const FString Output = Run(UTennisDebugSettings::TrajectoryCommandName, { TEXT("abc") });
		ASSERT_THAT(IsTrue(Output.Contains(TEXT("invalid"))));
		ASSERT_THAT(AreEqual(1, Settings->GetTrajectoryMode()));
	}

	TEST_METHOD(OutOfRangeValues_AreClamped)
	{
		Run(UTennisDebugSettings::TrajectoryCommandName, { TEXT("99") });
		Run(UTennisDebugSettings::TimeScaleCommandName, { TEXT("100") });
		ASSERT_THAT(AreEqual(UTennisDebugSettings::MaxTrajectoryMode, Settings->GetTrajectoryMode()));
		ASSERT_THAT(IsNear(UTennisDebugSettings::MaxTimeScale, Settings->GetTimeScale(), 1e-6f));

		// コマンドを通らない経路（MCP・ini）の範囲外の値も、読み出しで丸める
		SetByReflection(TEXT("TimeScale"), TEXT("-1"));
		ASSERT_THAT(IsNear(UTennisDebugSettings::MinTimeScale, Settings->GetTimeScale(), 1e-6f));
	}
};
