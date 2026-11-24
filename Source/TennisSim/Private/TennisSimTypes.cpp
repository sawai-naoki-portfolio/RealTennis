#include "TennisSimTypes.h"

#include "Math/RandomStream.h"

namespace
{
	// DD-01 §10 の定数
	constexpr double SeaLevelPressurePa = 101325.0;
	constexpr double IsaLapseFactor     = 2.25577e-5;  // 1/m
	constexpr double IsaExponent        = 5.25588;
	constexpr double DryAirGasConstant  = 287.05;      // J/(kg·K)
	constexpr double KelvinOffset       = 273.15;

	// NaN なら Fallback、そうでなければ範囲に丸める（入力の検証）
	double SafeClamp(double Value, double Lo, double Hi, double Fallback)
	{
		return FMath::IsNaN(Value) ? Fallback : FMath::Clamp(Value, Lo, Hi);
	}
}

double FMatchConditions::EffectiveTempC() const
{
	return bRoofClosed ? IndoorTempC : SafeClamp(TemperatureC, MinTempC, MaxTempC, FMatchConditions().TemperatureC);
}

FAirParams FMatchConditions::ComputeAir(int32 PointSeed) const
{
	FAirParams Air;
	Air.Rho = AirDensity(SafeClamp(AltitudeM, MinAltitudeM, MaxAltitudeM, 0.0), EffectiveTempC());
	Air.WindMean = (bRoofClosed || WindMean.ContainsNaN()) ? FVector::ZeroVector : WindMean;
	Air.GustFactor = SafeClamp(GustFactor, 0.0, MaxGustFactor, 0.0);

	FRandomStream Stream(PointSeed);  // 結果に関わる乱数はシードつき（ARCH-03 D-2）
	Air.GustPhase1 = Stream.GetFraction() * UE_DOUBLE_TWO_PI;
	Air.GustPhase2 = Stream.GetFraction() * UE_DOUBLE_TWO_PI;
	return Air;
}

double FMatchConditions::AirDensity(double AltitudeM, double TempC)
{
	const double Pressure = SeaLevelPressurePa * FMath::Pow(1.0 - IsaLapseFactor * AltitudeM, IsaExponent);
	return Pressure / (DryAirGasConstant * (TempC + KelvinOffset));
}
