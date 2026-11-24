#include "ContactMath.h"

TOptional<double> TennisContactMath::SweptCircleEntry(const FVector2D& A, const FVector2D& B, double D)
{
	// |A + sB| = D を解く
	const double Approach = FVector2D::DotProduct(A, B);  // 負なら近づいている
	if (Approach >= 0.0)
	{
		return {};
	}
	const double C = A.SizeSquared() - D * D;
	if (C <= 0.0)
	{
		return 0.0;  // 始めから円の中（めり込み）
	}
	const double Quad = B.SizeSquared();
	const double Disc = Approach * Approach - Quad * C;
	if (Disc < 0.0)
	{
		return {};
	}
	const double Fraction = (-Approach - FMath::Sqrt(Disc)) / Quad;
	return Fraction <= 1.0 ? TOptional<double>(Fraction) : TOptional<double>();
}

void TennisContactMath::ApplyImpulse(FVector& Vel, const FVector& Normal, double Restitution, double Friction)
{
	const double Vn = FVector::DotProduct(Vel, Normal);
	if (Vn >= 0.0)
	{
		return;
	}
	// 法線：反発 e。接線：摩擦 μ の撃力（接線の速さが 0 になるところまで）
	const FVector Tangent = Vel - Vn * Normal;
	const double TangentSpeed = Tangent.Size();
	const double FrictionDelta = FMath::Min(Friction * (1.0 + Restitution) * -Vn, TangentSpeed);
	Vel -= (1.0 + Restitution) * Vn * Normal;
	if (TangentSpeed > 0.0)
	{
		Vel -= (FrictionDelta / TangentSpeed) * Tangent;
	}
}
