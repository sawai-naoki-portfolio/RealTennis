#include "Core/TennisDebugSettings.h"

#include "HAL/IConsoleManager.h"
#include "Misc/DefaultValueHelper.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TennisDebugSettings)

namespace
{
	bool ParseNumber(const FString& Text, int32& Out) { return FDefaultValueHelper::ParseInt(Text, Out); }
	bool ParseNumber(const FString& Text, float& Out) { return FDefaultValueHelper::ParseFloat(Text, Out); }

	/** 引数なし：現在の値を表示。数値：範囲に丸めて書き込み、表示。それ以外：使い方を表示し、値は変えない */
	template <typename T>
	void HandleNumberCommand(const TArray<FString>& Args, FOutputDevice& Out, const TCHAR* Name, T& Value, T Min, T Max)
	{
		if (Args.Num() == 1)
		{
			T Parsed{};
			if (!ParseNumber(Args[0], Parsed))
			{
				Out.Logf(TEXT("%s: invalid value '%s'. Usage: %s [%s..%s]"), Name, *Args[0], Name,
					*LexToString(Min), *LexToString(Max));
				return;
			}
			Value = FMath::Clamp(Parsed, Min, Max);
		}
		else if (Args.Num() > 1)
		{
			Out.Logf(TEXT("Usage: %s [%s..%s]"), Name, *LexToString(Min), *LexToString(Max));
			return;
		}
		Out.Logf(TEXT("%s = %s"), Name, *LexToString(Value));
	}
}

UTennisDebugSettings::UTennisDebugSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("TennisDebug");
}

void UTennisDebugSettings::RegisterConsoleCommands()
{
#if !UE_BUILD_SHIPPING
	IConsoleManager& Console = IConsoleManager::Get();
	Console.RegisterConsoleCommand(TrajectoryCommandName,
		TEXT("Trajectory debug draw [0..2]. 0: off, 1: prediction and bounces, 2: also diff from live. No argument: show."),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, FOutputDevice& Out)
		{
			UTennisDebugSettings* Settings = GetMutableDefault<UTennisDebugSettings>();
			HandleNumberCommand(Args, Out, TrajectoryCommandName, Settings->DebugTrajectory, 0, MaxTrajectoryMode);
		}),
		ECVF_Cheat);
	Console.RegisterConsoleCommand(TimeScaleCommandName,
		TEXT("Simulation speed multiplier [0.05..8] (fixed step is kept). No argument: show."),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, FOutputDevice& Out)
		{
			UTennisDebugSettings* Settings = GetMutableDefault<UTennisDebugSettings>();
			HandleNumberCommand(Args, Out, TimeScaleCommandName, Settings->TimeScale, MinTimeScale, MaxTimeScale);
		}),
		ECVF_Cheat);
#endif
}

void UTennisDebugSettings::UnregisterConsoleCommands()
{
#if !UE_BUILD_SHIPPING
	IConsoleManager& Console = IConsoleManager::Get();
	for (const TCHAR* Name : { TrajectoryCommandName, TimeScaleCommandName })
	{
		if (IConsoleObject* Object = Console.FindConsoleObject(Name))
		{
			Console.UnregisterConsoleObject(Object);
		}
	}
#endif
}
