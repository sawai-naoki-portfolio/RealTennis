#include "Calibration/CourtCalibration.h"

#include "Ball/TennisBallPhysics.h"
#include "TennisConstants.h"

namespace
{
	constexpr double AwayFromNetX = -8.0;  // m（ネットの真上で落とすと白帯に当たる）
	constexpr double MaxDropSeconds = 3.0;
}

FCprResult TennisCourtCalibration::VirtualCprTest(const FSurfaceParams& Surface, const FBallParams& Ball)
{
	const double Angle = FMath::DegreesToRadians(CprAngleDeg);
	FBallState State;
	State.Vel = FVector(CprSpeed * FMath::Cos(Angle), 0.0, -CprSpeed * FMath::Sin(Angle));
	const FVector In = State.Vel;
	TennisBallPhysics::ResolveGroundBounce(State, Ball, Surface, FSurfaceParams::ReferenceTempC);

	FCprResult Result;
	Result.ApparentFriction = (In.X - State.Vel.X) / (-In.Z + State.Vel.Z);
	Result.ApparentRestitution = State.Vel.Z / -In.Z;
	Result.Cpr = 100.0 * (1.0 - Result.ApparentFriction) + 150.0 * (0.81 - Result.ApparentRestitution);
	Result.Category = CprCategory(Result.Cpr);
	return Result;
}

int32 TennisCourtCalibration::CprCategory(double Cpr)
{
	return Cpr < 30.0 ? 1 : Cpr < 35.0 ? 2 : Cpr < 40.0 ? 3 : Cpr < 45.0 ? 4 : 5;
}

double TennisCourtCalibration::DropTestReboundCm(const FBallParams& Ball, const FAirParams& Air)
{
	FBallSimContext Ctx;
	Ctx.Ball = Ball;
	Ctx.Air = Air;
	Ctx.Surface.RestitutionRef = FBallParams::Type2RestitutionHard;  // 硬い面：e = e_ball（速さ・気温によらない）
	Ctx.Surface.RestitutionSpeedSlope = 0.0;
	Ctx.Surface.TempCoeff = 0.0;

	FBallState State;
	State.Pos = FVector(AwayFromNetX, 0.0, DropHeightM + Ball.Radius);
	TArray<FBallEvent> Events;
	double Top = 0.0;
	const int32 MaxSteps = FMath::CeilToInt32(MaxDropSeconds / TennisConst::SimDt);
	for (int32 Step = 0; Step < MaxSteps; ++Step)
	{
		TennisBallPhysics::Step(State, TennisConst::SimDt, Ctx, Events);
		if (Events.Num() == 1)
		{
			Top = FMath::Max(Top, State.Pos.Z - Ball.Radius);
			if (State.Vel.Z < 0.0)
			{
				break;  // 頂点を過ぎた
			}
		}
	}
	return 100.0 * Top;
}
