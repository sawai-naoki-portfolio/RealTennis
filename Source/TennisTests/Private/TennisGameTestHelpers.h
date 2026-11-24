// Game のテスト（Tennis モジュールを使う）に共通の関数。Sim のテストは読み込まない（TennisSim は Engine に依存しない）。
#pragma once

#include "CoreMinimal.h"
#include "Core/TennisDebugSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/StringOutputDevice.h"

namespace TennisTest
{
	/** 時間の倍率を変える（コマンド tennis.TimeScale。値は 1 か所：ARCH-04 §3.1）。テストの後で元の値に戻す */
	inline void SetTimeScale(float Scale)
	{
		IConsoleObject* Object = IConsoleManager::Get().FindConsoleObject(UTennisDebugSettings::TimeScaleCommandName);
		check(Object && Object->AsCommand());
		FStringOutputDevice Out;
		Object->AsCommand()->Execute({ LexToString(Scale) }, nullptr, Out);
	}
}
