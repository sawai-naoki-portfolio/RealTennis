// ボールの物理（DD-01 §3、ARCH-02 §7）。予測・ソルバ・本番・ネットワーク・リプレイは、すべてここを通す（ARCH-03 D-7）。
#pragma once

#include "CoreMinimal.h"
#include "Ball/BallEvents.h"
#include "TennisSimTypes.h"

namespace TennisBallPhysics
{
	/**
	 * 1 ステップ（Dt 秒）進める（ACT-01）。空中を積分し、ステップの中で地面に届いたら、接地の時刻まで戻して
	 * バウンドを解き、残りの時間を続ける。イベントは時刻の順に OutEvents の後ろに加える（中身は消さない）。
	 * 予測・ソルバ・本番・ネットワーク・リプレイは、すべてこの関数で進める（ARCH-03 D-7）。
	 */
	TENNISSIM_API void Step(FBallState& State, double Dt, const FBallSimContext& Ctx, TArray<FBallEvent>& OutEvents);

	/** 時間を一度に進められる上限（s）。これより先へは AdvanceTo で進めない（呼び出し側の誤りを止める） */
	inline constexpr double MaxAdvanceTime = 10.0;

	/**
	 * TargetTime まで進める（DD-01 §3.4：過去の時刻からの打ち出しの早送り）。固定ステップで進め、最後の端数は
	 * 端数のステップで進める。終わった時刻はちょうど TargetTime。イベントは OutEvents の後ろに加える。
	 * TargetTime が今より前か、MaxAdvanceTime より先なら、状態を変えずに false（警告のログ）。
	 */
	TENNISSIM_API bool AdvanceTo(FBallState& State, double TargetTime, const FBallSimContext& Ctx, TArray<FBallEvent>& OutEvents);

	/** 加速度（m/s²）：重力＋抗力＋マグヌス。時刻 Time（s）の風（突風を含む）を引いた相対速度で計算する（DD-01 §3.1・§10）。 */
	TENNISSIM_API FVector ComputeAccel(const FVector& Vel, const FVector& Spin, const FBallParams& Ball, const FAirParams& Air, double Time);

	/**
	 * 空中の 1 ステップ：位置と速度を RK4 で Dt 秒進め、回転を減衰させ、時刻を進める（DD-01 §3.1・§3.2）。
	 * 衝突は扱わない（地面・ネット・ポストは Step が解く）。Dt は 0 以上（0 なら何も変わらない）。
	 * 負や NaN の Dt は呼び出し側の誤りで、状態を変えずにログの警告を出す。
	 */
	TENNISSIM_API void IntegrateFlight(FBallState& State, double Dt, const FBallSimContext& Ctx);

	/**
	 * 地面のバウンド（撃力。DD-01 §4.1、ARCH-03 §7）：速度と回転を変え、NumBounces を 1 増やす。位置は変えない。
	 * 反発は e(v_n, T) × (e_ball / e_ball(Type 2))、接線は μ を上限にグリップ（e_t 倍）か滑り。離れていく球（v_z ≥ 0）には何もしない。
	 */
	TENNISSIM_API void ResolveGroundBounce(FBallState& State, const FBallParams& Ball, const FSurfaceParams& Surface, double TempC);
}
