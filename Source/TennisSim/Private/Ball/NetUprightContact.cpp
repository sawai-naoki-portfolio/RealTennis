#include "NetUprightContact.h"

#include "ContactMath.h"
#include "TennisConstants.h"

namespace
{
	// DD-01 §5.4
	constexpr double UprightRestitution = 0.5;
	constexpr double UprightFriction    = 0.2;

	struct FUpright
	{
		double Y;       // m（X = 0 の上の位置）
		double Radius;  // m
		double Height;  // m
		ENetPart Part;
	};

	// DD-01 §5.1：スティックは高さ 1.07 m、ポストは 1.095 m
	constexpr FUpright Uprights[] = {
		{  TennisConst::SinglesStickY, TennisConst::StickRadius, TennisConst::NetHeightPost, ENetPart::SinglesStick },
		{ -TennisConst::SinglesStickY, TennisConst::StickRadius, TennisConst::NetHeightPost, ENetPart::SinglesStick },
		{  TennisConst::NetPostY,      TennisConst::PostRadius,  TennisConst::PostHeight,    ENetPart::Post },
		{ -TennisConst::NetPostY,      TennisConst::PostRadius,  TennisConst::PostHeight,    ENetPart::Post },
	};

	// 円柱の軸から見たボールの中心（XY 面）
	FVector2D OffsetFrom(const FUpright& Upright, const FVector& Pos)
	{
		return FVector2D(Pos.X, Pos.Y - Upright.Y);
	}
}

TOptional<double> TennisNetUprightContact::ContactFraction(const FBallState& Prev, const FBallState& Next, const FBallSimContext& Ctx)
{
	TOptional<double> First;
	for (const FUpright& Upright : Uprights)
	{
		const FVector2D Start = OffsetFrom(Upright, Prev.Pos);
		const TOptional<double> Fraction = TennisContactMath::SweptCircleEntry(
			Start, OffsetFrom(Upright, Next.Pos) - Start, Ctx.Ball.Radius + Upright.Radius);
		if (!Fraction.IsSet())
		{
			continue;
		}
		const double ZAtContact = Prev.Pos.Z + *Fraction * (Next.Pos.Z - Prev.Pos.Z);
		if (ZAtContact <= Upright.Height && (!First.IsSet() || *Fraction < *First))  // 上端より上を通る球は当たらない
		{
			First = Fraction;
		}
	}
	return First;
}

void TennisNetUprightContact::Resolve(FBallState& State, const FBallSimContext& Ctx, TArray<FBallEvent>& OutEvents)
{
	// いちばん近い（表面までの距離が最も小さい）円柱に当たっている
	const FUpright* Hit = nullptr;
	double BestGap = TNumericLimits<double>::Max();
	for (const FUpright& Upright : Uprights)
	{
		const double Gap = OffsetFrom(Upright, State.Pos).Size() - Upright.Radius;
		if (Gap < BestGap)
		{
			BestGap = Gap;
			Hit = &Upright;
		}
	}

	const FVector2D Offset = OffsetFrom(*Hit, State.Pos);
	const FVector2D Normal2D = Offset.IsNearlyZero() ? FVector2D(State.Vel.X > 0.0 ? -1.0 : 1.0, 0.0) : Offset.GetSafeNormal();
	const FVector Normal(Normal2D.X, Normal2D.Y, 0.0);
	if (FVector::DotProduct(State.Vel, Normal) >= 0.0)
	{
		return;  // 離れていく
	}

	TennisContactMath::ApplyImpulse(State.Vel, Normal, UprightRestitution, UprightFriction);

	// 円柱にちょうど触れる位置へ置き直す
	const double D = Ctx.Ball.Radius + Hit->Radius;
	State.Pos.X = Normal2D.X * D;
	State.Pos.Y = Hit->Y + Normal2D.Y * D;

	FBallEvent& Event = OutEvents.AddDefaulted_GetRef();
	Event.Type = EBallEventType::PostHit;
	Event.NetPart = Hit->Part;
	Event.Time = State.Time;
	Event.Pos = State.Pos;
	Event.Vel = State.Vel;
	Event.Spin = State.Spin;
}
