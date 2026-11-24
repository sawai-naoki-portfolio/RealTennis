#include "NetPlaneContact.h"

#include "Court/CourtGeometry.h"

namespace
{
	// ネット本体に当たったときの速度と回転の倍率（DD-01 §5.3）
	constexpr double BodyNormalFactor  = -0.1;  // v_x
	constexpr double BodyTangentFactor = 0.3;   // v_y・v_z
	constexpr double BodySpinFactor    = 0.2;
}

TOptional<double> TennisNetPlaneContact::CrossingFraction(const FBallState& Prev, const FBallState& Next)
{
	// 面の上から始まった球（直前に通した球）は、もう一度は数えない
	const bool bStartsOnPlane = Prev.Pos.X == 0.0;
	const bool bStaysOnSameSide = (Prev.Pos.X < 0.0) == (Next.Pos.X < 0.0) && Next.Pos.X != 0.0;
	if (bStartsOnPlane || bStaysOnSameSide)
	{
		return {};
	}
	return FMath::Clamp(Prev.Pos.X / (Prev.Pos.X - Next.Pos.X), 0.0, 1.0);
}

void TennisNetPlaneContact::Resolve(FBallState& State, const FBallSimContext& Ctx, TArray<FBallEvent>& OutEvents)
{
	const double Y = State.Pos.Y;
	if (!TennisCourtGeometry::HasNetAt(Y))
	{
		State.Pos.X = 0.0;
		return;  // ポストの外を回る球（Rule 25 c）
	}

	const double Radius = Ctx.Ball.Radius;
	const double Top = TennisCourtGeometry::NetHeightAt(Y);
	FBallEvent& Event = OutEvents.AddDefaulted_GetRef();
	Event.Time = State.Time;

	if (TennisCourtGeometry::HasNetBodyAt(Y) && State.Pos.Z + Radius < Top)
	{
		// 本体（網）：手前へ弱く跳ね返り、勢いを失う。位置は面から半径だけ手前に戻す
		const double Side = State.Vel.X > 0.0 ? -1.0 : 1.0;
		State.Vel.X *= BodyNormalFactor;
		State.Vel.Y *= BodyTangentFactor;
		State.Vel.Z *= BodyTangentFactor;
		State.Spin *= BodySpinFactor;
		State.Pos.X = Side * Radius;
		State.bNetTouched = true;
		Event.Type = EBallEventType::NetTouch;
		Event.NetPart = ENetPart::Body;
	}
	else
	{
		// 通過（スティックとポストの間で白帯の下を通る球も含む：Rule 25 d）。白帯に触れる球は、面に届く前に白帯の判定が解く
		State.Pos.X = 0.0;
		Event.Type = EBallEventType::NetCross;
		Event.NetClearance = State.Pos.Z - Radius - Top;
	}
	Event.Pos = State.Pos;
	Event.Vel = State.Vel;
	Event.Spin = State.Spin;
}
