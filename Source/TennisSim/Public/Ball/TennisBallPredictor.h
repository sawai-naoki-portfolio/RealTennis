// ボールの予測（DD-01 §3.5・§7）。本番と同じ TennisBallPhysics::Step で進めるので、同じステップ数なら状態は完全に一致する（D-7）。
#pragma once

#include "CoreMinimal.h"
#include "Ball/BallEvents.h"
#include "TennisSimTypes.h"
#include "TennisBallPredictor.generated.h"

struct FPredictOptions
{
	double MaxTime    = 4.0;  // s（打ち出しから）
	int32  MaxBounces = 2;    // このバウンドの数に達したら止める
};

/** 予測の結果：240 Hz の全状態（States[0] が打ち出し）と、その間のイベント。 */
USTRUCT()
struct TENNISSIM_API FTrajectory
{
	GENERATED_BODY()

	int32 Seq = 0;  // 打球の通し番号（打ち出しの状態から写す）
	TArray<FBallState> States;
	TArray<FBallEvent> Events;

	/** 最初のバウンド（なければ nullptr）。 */
	const FBallEvent* FirstBounce() const;
};

struct TENNISSIM_API FTennisBallPredictor
{
	/** Start から固定ステップで進め、Opt の打ち切り（時間・バウンドの数）まで Out に書く。Out の前の中身は消す。 */
	static void Predict(const FBallState& Start, const FBallSimContext& Ctx, const FPredictOptions& Opt, FTrajectory& Out);

	/**
	 * 予測の範囲の中の時刻 Time の状態（DD-01 §3.5）。Time の直前の状態から、端数だけ Step で進める（補間ではない）。
	 * 範囲の外（打ち出しより前・最後の状態より後）や空の予測なら false で、Out は変えない。
	 */
	static bool GetStateAt(const FTrajectory& Trajectory, double Time, const FBallSimContext& Ctx, FBallState& Out);
};
