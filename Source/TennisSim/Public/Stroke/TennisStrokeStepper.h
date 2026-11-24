// ストロークの進行（DD-03 §6.4・§6.6）：Engine に依存しない純粋な関数。モンタージュの位置・再生速度・体の向きを固定の刻み（SimDt）で進め、
// インパクト（正確な時刻）と終わり（RecoverStart）を出来事として返す。UTennisStrokeComponent は、結果をモンタージュと体に渡すだけ。
// 時間は s、角度は rad（コート局所のヨー）。
#pragma once

#include "CoreMinimal.h"
#include "Stroke/TennisStrokeTiming.h"

/** ストロークの状態（DD-03 §6.6） */
enum class EStrokeState : uint8
{
	None,           // 振っていない
	Waiting,        // 開始を遅らせている
	Swinging,       // 再生中。インパクトの前
	FollowThrough,  // インパクトから RecoverStart まで
};

/** モンタージュの時刻（先頭から、s。UStrokeData の焼いた値から作る） */
struct FStrokeClip
{
	double ImpactTime = 0.0;   // インパクト
	double RecoverTime = 0.0;  // RecoverStart（ImpactTime + FollowThroughLock）
};

/** 進行の状態 */
struct FStrokeStepper
{
	EStrokeState State = EStrokeState::None;
	FStrokeClip  Clip;
	double       Delay = 0.0;          // s（Waiting の残り）
	double       StartPosition = 0.0;  // s（再生を始めた位置。体の向きの進み具合の起点）
	double       Position = 0.0;       // s（モンタージュの位置）
	double       PlayRate = 1.0;
	int32        StepsSinceReplan = 0; // 前の読み直しからのステップ数
	double       StartYaw = 0.0;       // rad（始めの体の向き）
	double       TargetYaw = 0.0;      // rad（インパクトの体の向き：TennisStrokeTiming::FacingYaw）
	double       Yaw = 0.0;            // rad（今の体の向き）
};

/**
 * Advance の中で起きたこと。インパクトの時刻 T の体の向きは FStrokeStepper::TargetYaw（Advance が返った時点の Yaw は、
 * インパクトの後に進んだステップの分だけ、もうネットの方向へ戻り始めている）。
 */
struct FStrokeStepEvents
{
	bool   bImpact = false;
	double ImpactLead = 0.0;  // s（インパクトは、この Advance の終わりの時刻の ImpactLead 秒前。ステップの端数まで）
	bool   bEnded = false;    // RecoverStart に達した（状態は None に戻る）
};

namespace TennisStrokeStep
{
	/**
	 * ストロークを始める。インパクトが Available 秒後（打点の時刻 − 今のボールの時刻）になるように計画する（TennisStrokeTiming::PlanStart）。
	 * 体の向きは StartYaw から、インパクトで TargetYaw に着くように回す。Clip の時刻がおかしければ（0 < インパクト ≤ RecoverStart でない）、
	 * 始めずに false（警告を出す。Stepper は変えない：振っている途中ならそのまま）。
	 */
	TENNISSIM_API bool Begin(FStrokeStepper& Stepper, const FStrokeClip& Clip, double Available, double StartYaw, double TargetYaw,
		const FStrokeSyncParams& Params);

	/**
	 * NumSteps ステップ進める。AvailableAtEnd は、進めた後の「打点の時刻 − ボールの時刻」（読み直した値。過ぎていれば負）。
	 * Waiting の間は ReplanInterval ごとに計画し直し、Swinging の間は ReplanInterval ごとに再生速度を補正する（§6.1）。間隔は
	 * ReplanInterval を固定の刻みのステップ数に丸めて数える（0.05 s は 12 ステップ）。AvailableAtEnd が数でなければ、読み直さずに進める。
	 * 位置が ImpactTime を越えたらインパクト、RecoverTime を越えたら終わり。体の向きは、インパクトまでに TargetYaw へ、
	 * その後は RecoverStart までに NetYaw（ネットの方向）へ戻す（NetYaw が数でなければ、警告を出して TargetYaw のまま）。
	 * 振っていなければ・NumSteps が正でなければ、何もしない。
	 * 前提：ボールの時計と同じ NumSteps だけ進める（ボールと同じ刻みのアキュムレータで数えたステップ数を渡す）。各ステップの
	 * 「打点の時刻 − ボールの時刻」を AvailableAtEnd から逆算するため。この前提の下で、結果はフレームの分け方によらない。
	 */
	TENNISSIM_API FStrokeStepEvents Advance(FStrokeStepper& Stepper, int32 NumSteps, double AvailableAtEnd, double NetYaw, const FStrokeSyncParams& Params);

	/**
	 * 取り消す（DD-03 §6.6）。インパクトの前（Waiting・Swinging）なら、状態を None に戻して true（位置と向きはそのまま残る。
	 * 呼び出し側はモンタージュを止める）。インパクトの後（FollowThrough）は取り消さずに false を返し、Advance で終わりまで進める。
	 * 振っていなければ false。
	 */
	TENNISSIM_API bool Cancel(FStrokeStepper& Stepper);

	/**
	 * 下半身を Motion Matching と混ぜる重み（DD-03 §6.5。T-STRK-018）：振っていない・待つ間・テイクバック（モンタージュの位置 ≤
	 * ImpactTime − SwingLead − LowerBodyBlendTime）は 1、そこから ImpactTime − SwingLead までに線形に 0、その後（スイング・インパクト・
	 * フォロースルー）は 0。インパクトでは必ず 0（焼いたスイートスポットは全身のモンタージュの姿勢：DD-03 §7）。位置が数でなければ 0。
	 */
	TENNISSIM_API double LowerBodyLocomotionAlpha(const FStrokeStepper& Stepper, const FStrokeSyncParams& Params);
}
