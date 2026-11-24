#include "Shot/ShotRequestMapping.h"

#include "Shot/ShotSpin.h"
#include "TennisSimLog.h"

double TennisShotRequestMapping::ChargeAtImpact(const FShotIntent& Intent, double ChargeTime)
{
	if (!(ChargeTime > 0.0) || !FMath::IsFinite(ChargeTime))  // 設定の誤りは、押したままの（よく通る）経路でも知らせる
	{
		UE_LOG(LogTennisShot, Warning, TEXT("[Request] ChargeAtImpact: invalid charge_time=%f (using 1)"), ChargeTime);
		return 1.0;
	}
	if (Intent.bAutoReleased || !Intent.ReleaseTime.IsSet())
	{
		return 1.0;
	}
	if (!Intent.ChargeStartTime.IsSet() || !FMath::IsFinite(*Intent.ReleaseTime) || !FMath::IsFinite(*Intent.ChargeStartTime))
	{
		UE_LOG(LogTennisShot, Warning, TEXT("[Request] ChargeAtImpact: invalid input start=%s release=%f (using 1)"),
			Intent.ChargeStartTime.IsSet() ? *FString::SanitizeFloat(*Intent.ChargeStartTime) : TEXT("none"), *Intent.ReleaseTime);
		return 1.0;
	}
	return FMath::Clamp((*Intent.ReleaseTime - *Intent.ChargeStartTime) / ChargeTime, 0.0, 1.0);
}

FShotRequest TennisShotRequestMapping::Make(const FShotTypeParams& Params, const FVector& Origin, const FVector& Target, double Time,
	double Charge, ETennisHandedness Handedness, double SideInput)
{
	if (!FMath::IsFinite(Charge))
	{
		UE_LOG(LogTennisShot, Warning, TEXT("[Request] invalid Charge=%f (using 0)"), Charge);
		Charge = 0.0;
	}
	Charge = FMath::Clamp(Charge, 0.0, 1.0);

	FShotRequest Request;
	Request.Origin = Origin;
	Request.Target = Target;
	Request.Time = Time;
	Request.Constraint = Params.Constraint;
	Request.ConstraintValue = FMath::Lerp(Params.ConstraintAtCharge0, Params.ConstraintAtCharge1, Charge);
	Request.MinSpeed = Params.MinSpeed;
	Request.MaxSpeed = Params.MaxSpeed;
	if (Params.Constraint == ESolverConstraint::FixedSpeed)
	{
		Request.Speed = FMath::Lerp(Params.MinSpeed, Params.MaxSpeed, FMath::Pow(Charge, ChargeSpeedExponent));
	}
	Request.InitialElevationDeg = Params.InitialElevationDeg;
	Request.Rpm = Params.BaseRpm;
	Request.SpinAxisDeg = TennisShotSpin::AxisDeg(Params.SpinAxisDeg, Handedness, SideInput);
	return Request;
}
