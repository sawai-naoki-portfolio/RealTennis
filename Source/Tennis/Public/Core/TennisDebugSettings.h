// デバッグの切り替え（Project Settings → Game → Tennis Debug）。
// 値はこのオブジェクト（CDO）の 1 か所だけに置く。コンソールコマンド（tennis.*）は毎回このプロパティを直接読み書きし、
// 値のコピーを持たない。そのため、Unreal MCP の ConfigSettingsToolset で書いた値とコンソールで書いた値が食い違わない（ARCH-04 §3.1）。
// 範囲の検証は、書き込み（コマンド）と読み出し（Get*）の両方で行う（どの経路で入った値でも安全にする）。
// 開発者ごとの設定（DefaultConfig にしない）：MCP やエディタで変えた値は Saved/Config に保存され、コミットされない。
#pragma once

#include "Engine/DeveloperSettings.h"
#include "TennisDebugSettings.generated.h"

UCLASS(Config = Game, meta = (DisplayName = "Tennis Debug"))
class TENNIS_API UTennisDebugSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* TrajectoryCommandName = TEXT("tennis.Debug.Trajectory");
	static constexpr const TCHAR* TimeScaleCommandName = TEXT("tennis.TimeScale");

	static constexpr int32 MaxTrajectoryMode = 2;
	static constexpr float MinTimeScale = 0.05f;
	static constexpr float MaxTimeScale = 8.0f;

	UTennisDebugSettings();

	/** 0：描かない、1：予測軌道とバウンド点、2：本番の軌道との差も描く */
	int32 GetTrajectoryMode() const { return FMath::Clamp(DebugTrajectory, 0, MaxTrajectoryMode); }

	/** シミュレーションの速さの倍率（固定ステップは維持する） */
	float GetTimeScale() const { return FMath::Clamp(TimeScale, MinTimeScale, MaxTimeScale); }

	/** コンソールコマンドを登録・解除する。モジュールの起動と終了で呼ぶ。出荷ビルドでは何もしない */
	static void RegisterConsoleCommands();
	static void UnregisterConsoleCommands();

private:
	/** tennis.Debug.Trajectory（meta の範囲は MaxTrajectoryMode と同じ） */
	UPROPERTY(Config, EditAnywhere, Category = "Draw", meta = (ClampMin = 0, ClampMax = 2))
	int32 DebugTrajectory = 0;

	/** tennis.TimeScale（meta の範囲は MinTimeScale・MaxTimeScale と同じ） */
	UPROPERTY(Config, EditAnywhere, Category = "Simulation", meta = (ClampMin = 0.05, ClampMax = 8.0))
	float TimeScale = 1.0f;
};
