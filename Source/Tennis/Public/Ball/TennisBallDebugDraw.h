// ボールのデバッグの描画（W-1.8、ARCH-03 §5.2 の tennis.Debug.Trajectory）。見た目だけで、ゲームの状態は変えない。
// 出荷版では描かない（ENABLE_DRAW_DEBUG）。
#pragma once

#include "CoreMinimal.h"

struct FTrajectory;
class ATennisCourt;
class UWorld;

namespace TennisBallDebugDraw
{
	/** 予測の状態を Stride ごとに間引いて、ワールドの点列（cm）にする。最後の状態は必ず入れる。Stride が 1 未満なら 1。 */
	TENNIS_API TArray<FVector> TrajectoryPolyline(const FTrajectory& Trajectory, const FTransform& CourtTransform, int32 Stride);

	/** 予測の軌道（黄）とバウンド点（赤の球）・ネットの通過（緑）・ネットとポスト（橙）を 1 フレームだけ描く。 */
	TENNIS_API void DrawPrediction(const UWorld* World, const ATennisCourt& Court, const FTrajectory& Trajectory);

	/** 本番の軌道（水色。コート局所の位置の列）を 1 フレームだけ描く。予測との差を見るため（モード 2）。 */
	TENNIS_API void DrawActualPath(const UWorld* World, const ATennisCourt& Court, const TArray<FVector>& LocalPath);
}
