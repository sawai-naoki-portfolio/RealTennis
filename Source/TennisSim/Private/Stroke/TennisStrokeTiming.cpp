#include "Stroke/TennisStrokeTiming.h"

#include "TennisSimLog.h"

namespace
{
	/** 範囲の値がおかしくても（下限 > 上限、0 以下）、使える範囲にする */
	void RateRange(const FStrokeSyncParams& Params, double& OutMin, double& OutMax)
	{
		OutMin = FMath::Max(Params.MinPlayRate, UE_DOUBLE_KINDA_SMALL_NUMBER);
		OutMax = FMath::Max(Params.MaxPlayRate, OutMin);
	}
}

double TennisStrokeTiming::WrapAngle(double Angle)
{
	const double Wrapped = FMath::Fmod(Angle + UE_DOUBLE_PI, UE_DOUBLE_TWO_PI);
	return (Wrapped <= 0.0 ? Wrapped + UE_DOUBLE_TWO_PI : Wrapped) - UE_DOUBLE_PI;
}

double TennisStrokeTiming::NetYaw(double SideSign)
{
	return SideSign < 0.0 ? 0.0 : UE_DOUBLE_PI;
}

FVector2D TennisStrokeTiming::NeutralAim(double SideSign, const FStrokeSyncParams& Params)
{
	const double Opponent = SideSign < 0.0 ? 1.0 : -1.0;  // 相手のコートの向き
	return FVector2D(Opponent * FMath::Max(Params.NeutralAimDepth, 0.0), 0.0);
}

FStrokeStartPlan TennisStrokeTiming::PlanStart(double ImpactTime, double Available, const FStrokeSyncParams& Params)
{
	FStrokeStartPlan Plan;
	if (!(ImpactTime > 0.0) || !FMath::IsFinite(ImpactTime) || !FMath::IsFinite(Available))
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Timing] PlanStart: invalid input impact_time=%.3f available=%.3f (playing from the start at rate 1)"),
			ImpactTime, Available);
		return Plan;
	}
	double MinRate = 0.0;
	double MaxRate = 0.0;
	RateRange(Params, MinRate, MaxRate);

	if (Available <= 0.0)  // 打点の時刻を過ぎた：インパクトの位置から
	{
		Plan.StartPosition = ImpactTime;
		Plan.PlayRate = MaxRate;
		return Plan;
	}
	const double Needed = ImpactTime / Available;
	if (Needed > MaxRate)  // 時間が足りない：途中から（テイクバックを省く）
	{
		Plan.PlayRate = MaxRate;
		Plan.StartPosition = ImpactTime - Available * MaxRate;
	}
	else if (Needed < MinRate)  // 時間が余る：開始を遅らせる
	{
		Plan.PlayRate = MinRate;
		Plan.Delay = Available - ImpactTime / MinRate;
	}
	else
	{
		Plan.PlayRate = Needed;
	}
	return Plan;
}

double TennisStrokeTiming::CorrectPlayRate(double CurrentRate, double Position, double ImpactTime, double Available, double Elapsed,
	const FStrokeSyncParams& Params)
{
	if (!FMath::IsFinite(CurrentRate))
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Timing] CorrectPlayRate: invalid current rate (using 1)"));
		CurrentRate = 1.0;
	}
	const double Remaining = ImpactTime - Position;
	if (!(Remaining > 0.0) || !(Elapsed > 0.0) || !FMath::IsFinite(Available))
	{
		return CurrentRate;  // インパクトを過ぎた・進んでいない・数でない：変えない
	}
	double MinRate = 0.0;
	double MaxRate = 0.0;
	RateRange(Params, MinRate, MaxRate);
	const double Desired = Available > 0.0 ? FMath::Clamp(Remaining / Available, MinRate, MaxRate) : MaxRate;
	const double MaxChange = FMath::Max(Params.MaxRateChangePerSecond, 0.0) * Elapsed;
	return FMath::Clamp(CurrentRate + FMath::Clamp(Desired - CurrentRate, -MaxChange, MaxChange), MinRate, MaxRate);
}

double TennisStrokeTiming::FacingYaw(const FVector2D& HitPoint, const FVector2D& AimPoint, double SideSign, const FStrokeSyncParams& Params)
{
	const double Net = NetYaw(SideSign);
	const FVector2D ToAim = AimPoint - HitPoint;
	if (!FMath::IsFinite(ToAim.X) || !FMath::IsFinite(ToAim.Y) || ToAim.SizeSquared() <= UE_DOUBLE_SMALL_NUMBER)
	{
		return Net;
	}
	const double Limit = FMath::DegreesToRadians(FMath::Clamp(Params.MaxFacingDeg, 0.0, 90.0));  // データは度、計算は rad（ARCH-03 U-6）
	const double FromNet = FMath::Clamp(WrapAngle(FMath::Atan2(ToAim.Y, ToAim.X) - Net), -Limit, Limit);
	return WrapAngle(Net + FromNet);
}

double TennisStrokeTiming::ReturnYaw(double Yaw, double TargetYaw, double RateRad, double Dt)
{
	if (!FMath::IsFinite(TargetYaw))
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Timing] ReturnYaw: invalid target_yaw=%f (keeping yaw=%f)"), TargetYaw, Yaw);
		return FMath::IsFinite(Yaw) ? WrapAngle(Yaw) : 0.0;
	}
	if (!FMath::IsFinite(Yaw))
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Timing] ReturnYaw: invalid yaw=%f (using target_yaw=%f)"), Yaw, TargetYaw);
		return WrapAngle(TargetYaw);
	}
	const double MaxStep = RateRad * Dt;
	if (!(MaxStep > 0.0))
	{
		return WrapAngle(Yaw);
	}
	const double Delta = WrapAngle(TargetYaw - Yaw);
	return FMath::Abs(Delta) <= MaxStep ? WrapAngle(TargetYaw) : WrapAngle(Yaw + (Delta > 0.0 ? MaxStep : -MaxStep));
}

FVector2D TennisStrokeTiming::BodyTarget(const FVector2D& BallPos, double YawRad, const FVector2D& SweetSpotOffset)
{
	double Sin = 0.0;
	double Cos = 1.0;
	FMath::SinCos(&Sin, &Cos, YawRad);
	const FVector2D Forward(Cos, Sin);
	const FVector2D Right(-Sin, Cos);  // +X を向いたとき、右は +Y
	return BallPos - Forward * SweetSpotOffset.X - Right * SweetSpotOffset.Y;
}
