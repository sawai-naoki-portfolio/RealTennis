#include "Stroke/TennisStrokeStepper.h"

#include "TennisConstants.h"
#include "TennisSimLog.h"

namespace
{
	/** From から To へ、近い回りで Alpha（0〜1）だけ回した向き */
	double TurnToward(double From, double To, double Alpha)
	{
		return TennisStrokeTiming::WrapAngle(From + TennisStrokeTiming::WrapAngle(To - From) * FMath::Clamp(Alpha, 0.0, 1.0));
	}

	void ApplyPlan(FStrokeStepper& Stepper, const FStrokeStartPlan& Plan)
	{
		Stepper.Delay = Plan.Delay;
		Stepper.StartPosition = Plan.StartPosition;
		Stepper.Position = Plan.StartPosition;
		Stepper.PlayRate = Plan.PlayRate;
	}

	/** 待ち終わった：残りの時間の分（負の Delay）だけ進めて、再生を始める */
	void StartSwing(FStrokeStepper& Stepper)
	{
		Stepper.State = EStrokeState::Swinging;
		Stepper.Position = Stepper.StartPosition + Stepper.PlayRate * FMath::Max(-Stepper.Delay, 0.0);
		Stepper.Delay = 0.0;
		Stepper.StepsSinceReplan = 0;
	}

	/** 読み直しの間隔（ステップ数）：ReplanInterval を固定の刻みに丸める（0.05 s は 12 ステップ。時間を足し合わせると誤差で 1 ステップずれる） */
	int32 ReplanSteps(const FStrokeSyncParams& Params)
	{
		const double Steps = Params.ReplanInterval / TennisConst::SimDt;
		return FMath::IsNaN(Steps) ? 1 : FMath::RoundToInt32(FMath::Clamp(Steps, 1.0, 1.0e6));  // +∞ も大きな値と同じく上限で止める
	}
}

bool TennisStrokeStep::Begin(FStrokeStepper& Stepper, const FStrokeClip& Clip, double Available, double StartYaw, double TargetYaw,
	const FStrokeSyncParams& Params)
{
	if (!(Clip.ImpactTime > 0.0) || !(Clip.RecoverTime >= Clip.ImpactTime) || !FMath::IsFinite(Clip.RecoverTime) || !FMath::IsFinite(Available)
		|| !FMath::IsFinite(StartYaw) || !FMath::IsFinite(TargetYaw))
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Stroke] Begin: invalid input impact_time=%.3f recover_time=%.3f available=%.3f (not started)"),
			Clip.ImpactTime, Clip.RecoverTime, Available);
		return false;
	}
	Stepper = FStrokeStepper();
	Stepper.Clip = Clip;
	Stepper.StartYaw = TennisStrokeTiming::WrapAngle(StartYaw);
	Stepper.TargetYaw = TennisStrokeTiming::WrapAngle(TargetYaw);
	Stepper.Yaw = Stepper.StartYaw;
	ApplyPlan(Stepper, TennisStrokeTiming::PlanStart(Clip.ImpactTime, Available, Params));
	Stepper.State = EStrokeState::Waiting;
	if (Stepper.Delay <= 0.0)
	{
		StartSwing(Stepper);
	}
	return true;
}

FStrokeStepEvents TennisStrokeStep::Advance(FStrokeStepper& Stepper, int32 NumSteps, double AvailableAtEnd, double NetYaw, const FStrokeSyncParams& Params)
{
	FStrokeStepEvents Events;
	if (Stepper.State == EStrokeState::None || NumSteps <= 0)
	{
		return Events;
	}
	if (!FMath::IsFinite(NetYaw))
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Stroke] Advance: invalid net yaw (keeping the target yaw)"));
		NetYaw = Stepper.TargetYaw;
	}
	constexpr double Dt = TennisConst::SimDt;
	const int32 ReplanEvery = ReplanSteps(Params);
	for (int32 Step = 0; Step < NumSteps && Stepper.State != EStrokeState::None; ++Step)
	{
		const double TimeAfterStep = (NumSteps - Step - 1) * Dt;     // このステップの終わりから、Advance の終わりまで
		const double Available = AvailableAtEnd + TimeAfterStep + Dt;  // このステップの始めの「打点の時刻 − ボールの時刻」
		++Stepper.StepsSinceReplan;
		const bool bReplan = Stepper.StepsSinceReplan >= ReplanEvery && FMath::IsFinite(Available);

		if (Stepper.State == EStrokeState::Waiting)
		{
			if (bReplan)  // 打点の時刻が変わっていれば、待つ時間・開始位置・再生速度を決め直す
			{
				ApplyPlan(Stepper, TennisStrokeTiming::PlanStart(Stepper.Clip.ImpactTime, Available, Params));
				Stepper.StepsSinceReplan = 0;
			}
			Stepper.Delay -= Dt;
			if (Stepper.Delay > 0.0)
			{
				continue;
			}
			StartSwing(Stepper);
		}
		else
		{
			if (bReplan && Stepper.State == EStrokeState::Swinging)
			{
				Stepper.PlayRate = TennisStrokeTiming::CorrectPlayRate(Stepper.PlayRate, Stepper.Position, Stepper.Clip.ImpactTime, Available,
					Stepper.StepsSinceReplan * Dt, Params);
				Stepper.StepsSinceReplan = 0;
			}
			Stepper.Position += Stepper.PlayRate * Dt;
		}

		if (Stepper.State == EStrokeState::Swinging)
		{
			const double Span = Stepper.Clip.ImpactTime - Stepper.StartPosition;
			Stepper.Yaw = TurnToward(Stepper.StartYaw, Stepper.TargetYaw, Span > 0.0 ? (Stepper.Position - Stepper.StartPosition) / Span : 1.0);
			if (Stepper.Position >= Stepper.Clip.ImpactTime)
			{
				Events.bImpact = true;
				Events.ImpactLead = TimeAfterStep + (Stepper.Position - Stepper.Clip.ImpactTime) / Stepper.PlayRate;
				Stepper.State = EStrokeState::FollowThrough;
			}
		}
		if (Stepper.State == EStrokeState::FollowThrough)
		{
			const double Span = Stepper.Clip.RecoverTime - Stepper.Clip.ImpactTime;
			Stepper.Yaw = TurnToward(Stepper.TargetYaw, NetYaw, Span > 0.0 ? (Stepper.Position - Stepper.Clip.ImpactTime) / Span : 1.0);
			if (Stepper.Position >= Stepper.Clip.RecoverTime)
			{
				Events.bEnded = true;
				Stepper.State = EStrokeState::None;
			}
		}
	}
	return Events;
}

double TennisStrokeStep::LowerBodyLocomotionAlpha(const FStrokeStepper& Stepper, const FStrokeSyncParams& Params)
{
	switch (Stepper.State)
	{
	case EStrokeState::None:
	case EStrokeState::Waiting:
		return 1.0;  // 足は Motion Matching（待つ間のモンタージュは構えの姿勢のまま）
	case EStrokeState::FollowThrough:
		return 0.0;
	case EStrokeState::Swinging:
		break;
	}
	const double SwingStart = Stepper.Clip.ImpactTime - FMath::Max(Params.SwingLead, 0.0);
	if (!(Stepper.Position < SwingStart))  // スイングから（位置が数でないときも）全身：インパクトの姿勢を守る
	{
		return 0.0;
	}
	const double Blend = FMath::Max(Params.LowerBodyBlendTime, 0.0);
	return Blend > 0.0 ? FMath::Clamp((SwingStart - Stepper.Position) / Blend, 0.0, 1.0) : 1.0;
}

bool TennisStrokeStep::Cancel(FStrokeStepper& Stepper)
{
	if (Stepper.State != EStrokeState::Waiting && Stepper.State != EStrokeState::Swinging)
	{
		return false;  // 振っていない・インパクトの後（終わりまで進める：DD-03 §6.6）
	}
	Stepper.State = EStrokeState::None;
	return true;
}
