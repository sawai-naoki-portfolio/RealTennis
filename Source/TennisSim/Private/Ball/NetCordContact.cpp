#include "NetCordContact.h"

#include "ContactMath.h"
#include "Court/CourtGeometry.h"

namespace
{
	// DD-01 §5.2 の白帯のモデル
	constexpr double CordRadius      = 0.006;  // m
	constexpr double CordRestitution = 0.30;
	constexpr double CordFriction    = 0.4;

	// 白帯の軸から見たボールの中心（XZ 面）
	FVector2D OffsetFromCord(const FVector& Pos)
	{
		return FVector2D(Pos.X, Pos.Z - TennisCourtGeometry::NetHeightAt(Pos.Y));
	}
}

TOptional<double> TennisNetCordContact::ContactFraction(const FBallState& Prev, const FBallState& Next, const FBallSimContext& Ctx)
{
	// 白帯の高さの変化（Y が動く分）も、ステップの間の動きに含める
	const FVector2D Start = OffsetFromCord(Prev.Pos);
	const TOptional<double> Fraction = TennisContactMath::SweptCircleEntry(
		Start, OffsetFromCord(Next.Pos) - Start, Ctx.Ball.Radius + CordRadius);
	if (!Fraction.IsSet())
	{
		return {};
	}
	const double YAtContact = Prev.Pos.Y + *Fraction * (Next.Pos.Y - Prev.Pos.Y);
	return TennisCourtGeometry::HasNetAt(YAtContact) ? Fraction : TOptional<double>();  // ポストの外に白帯はない（Rule 25 c）
}

void TennisNetCordContact::Resolve(FBallState& State, const FBallSimContext& Ctx, TArray<FBallEvent>& OutEvents)
{
	const FVector2D Offset = OffsetFromCord(State.Pos);
	const FVector2D Normal2D = Offset.IsNearlyZero() ? FVector2D(0.0, 1.0) : Offset.GetSafeNormal();
	const FVector Normal(Normal2D.X, 0.0, Normal2D.Y);
	if (FVector::DotProduct(State.Vel, Normal) >= 0.0)
	{
		return;  // 離れていく
	}

	TennisContactMath::ApplyImpulse(State.Vel, Normal, CordRestitution, CordFriction);  // 回転は変えない（DD-01 §5.2）

	// 白帯にちょうど触れる位置へ置き直す（積分の誤差でめり込んだままにしない）
	const double D = Ctx.Ball.Radius + CordRadius;
	State.Pos.X = Normal2D.X * D;
	State.Pos.Z = TennisCourtGeometry::NetHeightAt(State.Pos.Y) + Normal2D.Y * D;
	State.bNetTouched = true;

	FBallEvent& Event = OutEvents.AddDefaulted_GetRef();
	Event.Type = EBallEventType::NetTouch;
	Event.NetPart = ENetPart::Cord;
	Event.Time = State.Time;
	Event.Pos = State.Pos;
	Event.Vel = State.Vel;
	Event.Spin = State.Spin;
}
