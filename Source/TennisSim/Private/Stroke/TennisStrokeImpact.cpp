#include "Stroke/TennisStrokeImpact.h"

FVector TennisStrokeImpact::HandedOffset(const FVector& SweetSpotOffset, ETennisHandedness Handedness)
{
	return FVector(SweetSpotOffset.X, Handedness == ETennisHandedness::Right ? SweetSpotOffset.Y : -SweetSpotOffset.Y, SweetSpotOffset.Z);
}

FVector2D TennisStrokeImpact::BodyAt(const FVector2D& FrameStartPos, const FVector2D& Velocity, double FrameStartTime, double T)
{
	return FrameStartPos + Velocity * (T - FrameStartTime);
}

FVector TennisStrokeImpact::SweetSpotAt(const FVector2D& BodyPos, double YawRad, const FVector& Offset)
{
	double Sin = 0.0;
	double Cos = 1.0;
	FMath::SinCos(&Sin, &Cos, YawRad);
	const FVector2D Forward(Cos, Sin);
	const FVector2D Right(-Sin, Cos);  // +X を向いたとき、右は +Y（BodyTarget と同じ）
	const FVector2D Horizontal = BodyPos + Forward * Offset.X + Right * Offset.Y;
	return FVector(Horizontal.X, Horizontal.Y, Offset.Z);
}

bool TennisStrokeImpact::IsWithinReach(double Distance, const FStrokeSyncParams& Params)
{
	return Distance <= Params.ImpactReach;  // 数でない距離・判定の距離は false
}
