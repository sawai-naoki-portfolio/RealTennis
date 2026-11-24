#include "ShotInitialGuess.h"

#include "Ball/TennisBallPhysics.h"
#include "Court/CourtGeometry.h"
#include "Shot/TennisShotSolver.h"
#include "TennisConstants.h"

using TennisShotInitialGuess::FGuess;

namespace
{
	constexpr int32  Refinements = 3;         // 近似の係数を、見積もった (s, θ) で測り直す回数
	constexpr double MinModelSpeed = 1.0;     // m/s（これより遅い見積もりでは、空気の力を測らない）
	constexpr double MinGravityFactor = 0.25; // 逆回転の揚力で、実効の重力がこれより小さくならないようにする（g に対する割合）

	// 近似の軌道。抗力は、水平の速さが距離に対して指数で減る（v = v0·e^{−kx}）とみなし、マグヌスの縦の成分は重力に含める（g'）。
	// すると時刻は t = u(x) / (s cosθ)、u(x) = (e^{kx} − 1) / k になり、軌道は u についての放物線 z = h + u tanθ − g' u² / (2 s² cos²θ)
	struct FModel
	{
		double DragPerMeter = 0.0;              // k（1/m）。0 なら抗力なし
		double Gravity = TennisConst::Gravity;  // g'（m/s²）

		double Stretch(double X) const
		{
			return DragPerMeter > UE_DOUBLE_SMALL_NUMBER ? (FMath::Exp(DragPerMeter * X) - 1.0) / DragPerMeter : X;
		}
	};

	// 打ち出し (s, θ) について、本番と同じ空気の力（ComputeAccel）から k と g' を測る。速さが落ちた、飛行の中ほどの値を使う
	FModel MeasureModel(const FShotRequest& Request, double Azimuth, const FGuess& At, double Distance, const FBallSimContext& Ctx)
	{
		if (At.Speed < MinModelSpeed)
		{
			return FModel();
		}
		const FBallState Launch = FTennisShotSolver::MakeLaunch(Request, Azimuth, At.Speed, At.Elevation);
		const FVector Dir = Launch.Vel / At.Speed;
		const FVector Gravity(0.0, 0.0, -TennisConst::Gravity);
		const auto AeroAt = [&](const FVector& Vel)
		{
			return TennisBallPhysics::ComputeAccel(Vel, Launch.Spin, Ctx.Ball, Ctx.Air, Launch.Time) - Gravity;
		};
		const double LaunchDrag = FMath::Max(-FVector::DotProduct(AeroAt(Launch.Vel), Dir), 0.0) / (At.Speed * At.Speed);
		const FVector MidVel = Launch.Vel * FMath::Exp(-0.5 * LaunchDrag * Distance);
		const double MidSpeed = MidVel.Size();
		const FVector Aero = AeroAt(MidVel);
		const double AlongDir = FVector::DotProduct(Aero, Dir);

		FModel Model;
		Model.DragPerMeter = FMath::Max(-AlongDir, 0.0) / (MidSpeed * MidSpeed);
		Model.Gravity = FMath::Max(TennisConst::Gravity - (Aero - AlongDir * Dir).Z, MinGravityFactor * TennisConst::Gravity);
		return Model;
	}

	// ネットの上端 NetTop（中心の高さ）を越えて、距離 Distance で中心が Ground の高さに下りる (s, θ)。
	// 2 つの条件は A = tanθ、B = g' / (2 s² cos²θ) について連立一次方程式になる。解けなければ値なし
	TOptional<FGuess> SolveClearance(const FModel& Model, double Height, double NetDistance, double NetTop, double Distance, double Ground)
	{
		if (NetDistance <= 0.0 || NetDistance >= Distance)
		{
			return {};
		}
		const double Un = Model.Stretch(NetDistance);
		const double Ud = Model.Stretch(Distance);
		const double Det = Ud * Un * Un - Un * Ud * Ud;  // = un·ud·(un − ud)
		if (FMath::Abs(Det) < UE_DOUBLE_SMALL_NUMBER)
		{
			return {};
		}
		// h + un A − un² B = NetTop、h + ud A − ud² B = Ground
		const double R1 = NetTop - Height;
		const double R2 = Ground - Height;
		const double A = (R1 * -Ud * Ud + Un * Un * R2) / Det;
		const double B = (Un * R2 - Ud * R1) / Det;
		if (B <= 0.0)
		{
			return {};
		}
		const double Elevation = FMath::Atan(A);
		const double Cos = FMath::Cos(Elevation);
		return FGuess{ FMath::Sqrt(Model.Gravity / (2.0 * B * Cos * Cos)), Elevation };
	}

	// 仰角 θ で、高さ Drop だけ下りて距離 Distance に届く速さ（届かない向きなら 0）
	double SpeedFor(const FModel& Model, double Distance, double Drop, double Elevation)
	{
		const double U = Model.Stretch(Distance);
		const double Fall = Drop + U * FMath::Tan(Elevation);
		const double Cos = FMath::Cos(Elevation);
		return Fall > UE_DOUBLE_SMALL_NUMBER ? FMath::Sqrt(Model.Gravity * U * U / (2.0 * Cos * Cos * Fall)) : 0.0;
	}
}

FGuess TennisShotInitialGuess::Estimate(const FShotRequest& Request, double Azimuth, double ConstraintValue, const FBallSimContext& Ctx)
{
	const double RequestElevation = FMath::DegreesToRadians(Request.InitialElevationDeg);
	FGuess Guess{ Request.Speed, RequestElevation };
	if (Request.Constraint == ESolverConstraint::FixedSpeed)
	{
		return Guess;
	}

	const double Radius = Ctx.Ball.Radius;
	const double Distance = FVector::Dist2D(Request.Origin, Request.Target);
	const FVector Dir(FMath::Cos(Azimuth), FMath::Sin(Azimuth), 0.0);
	const bool bClearance = Request.Constraint == ESolverConstraint::NetClearance && FMath::Abs(Dir.X) > UE_DOUBLE_SMALL_NUMBER;
	const double NetDistance = bClearance ? -Request.Origin.X / Dir.X : 0.0;  // 方位に沿ったネットの面までの距離
	const double NetTop = bClearance
		? TennisCourtGeometry::NetHeightAt(Request.Origin.Y + Dir.Y * NetDistance) + Radius + ConstraintValue
		: 0.0;

	FModel Model;  // 最初は空気の力なし。見積もった (s, θ) で係数を測り直す
	for (int32 Pass = 0; Pass <= Refinements; ++Pass)
	{
		if (Pass > 0)
		{
			Model = MeasureModel(Request, Azimuth, Guess, Distance, Ctx);
		}
		const TOptional<FGuess> ByClearance = bClearance
			? SolveClearance(Model, Request.Origin.Z, NetDistance, NetTop, Distance, Radius)
			: TOptional<FGuess>();
		Guess = ByClearance.IsSet()
			? ByClearance.GetValue()
			: FGuess{ SpeedFor(Model, Distance, Request.Origin.Z - Radius, RequestElevation), RequestElevation };
	}
	return Guess;
}
