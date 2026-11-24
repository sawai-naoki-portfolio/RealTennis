#include "Shot/TennisShotSolver.h"

#include "Ball/TennisBallPhysics.h"
#include "Court/CourtGeometry.h"
#include "ShotInitialGuess.h"
#include "TennisConstants.h"
#include "TennisUnits.h"

namespace
{
	// DD-02 §7.3 の反復の規則
	constexpr int32  MaxNewtonIterations = 10;
	constexpr int32  MaxAzimuthPasses = 3;
	constexpr double RangeTolerance = 0.05;       // m
	constexpr double ConstraintTolerance = 0.05;  // m
	constexpr double LateralTolerance = 0.05;     // m
	constexpr double SpeedDelta = 0.05;           // m/s（差分）
	constexpr double ElevationDelta = 0.002;      // rad（差分）
	constexpr int32  MaxBacktracks = 3;           // 1 歩で残差が悪くなれば歩幅を半分に（最大 3 回。別の解へ跳ばない）
	constexpr int32  MaxIterationsAtSpeedLimit = 3;  // 速さが範囲の端に続けて張り付いたら、この条件では解けないとみなす
	constexpr double MinSpeedFactor = 0.8;        // s の範囲：[MinSpeed × 0.8, MaxSpeed × 1.1]
	constexpr double MaxSpeedFactor = 1.1;
	constexpr double MinElevation = -25.0 * UE_DOUBLE_PI / 180.0;  // rad
	constexpr double MaxElevation = 60.0 * UE_DOUBLE_PI / 180.0;   // rad
	constexpr double MaxFlightSeconds = 4.0;      // s（1 回の試行の長さ）
	constexpr double RelaxDownFactor = 0.5;       // DD-02 §7.4 RelaxConstraint：条件を半分に
	constexpr double RelaxUpFactor = 1.5;         // DD-02 §7.4 SpeedClamped：条件を大きく（速さを抑える）

	struct FTrial
	{
		double Speed = 0.0;
		double Elevation = 0.0;
		double RangeResidual = 0.0;       // m（方位に沿った飛距離 − 目標の距離）
		double LateralResidual = 0.0;     // m（方位の左右へのずれ。左が正）
		double ConstraintResidual = 0.0;  // m
		FTennisShotSolver::FFlight Flight;

		bool IsConverged() const
		{
			return FMath::Abs(RangeResidual) < RangeTolerance && FMath::Abs(ConstraintResidual) < ConstraintTolerance;
		}

		double Score() const
		{
			return FMath::Abs(RangeResidual) + FMath::Abs(LateralResidual) + FMath::Abs(ConstraintResidual);
		}
	};

	FVector Heading(double Azimuth)
	{
		return FVector(FMath::Cos(Azimuth), FMath::Sin(Azimuth), 0.0);
	}
}

FTennisShotSolver::FTennisShotSolver(const FBallSimContext& InContext)
	: Context(InContext)
{
	Context.Air.GustFactor = 0.0;  // ソルバが知る風は平均だけ（FR-ENV-004）
}

FBallState FTennisShotSolver::MakeLaunch(const FShotRequest& Request, double Azimuth, double Speed, double Elevation)
{
	const FVector H = Heading(Azimuth);
	const FVector Up(0.0, 0.0, 1.0);
	const double Phi = FMath::DegreesToRadians(Request.SpinAxisDeg);
	const double Omega = TennisUnits::RpmToRadPerSec(Request.Rpm);

	FBallState Launch;
	Launch.Pos = Request.Origin;
	Launch.Vel = Speed * (FMath::Cos(Elevation) * H + FMath::Sin(Elevation) * Up);
	Launch.Spin = Omega * (FMath::Cos(Phi) * FVector::CrossProduct(Up, H) + FMath::Sin(Phi) * Up);  // DD-02 §3.1
	Launch.Time = Request.Time;
	Launch.bInPlay = true;
	return Launch;
}

FTennisShotSolver::FFlight FTennisShotSolver::SimulateToFirstBounce(const FBallState& Launch) const
{
	FFlight Flight;
	FBallState State = Launch;
	Flight.Apex = State.Pos.Z;
	TArray<FBallEvent> Events;
	const int32 MaxSteps = FMath::CeilToInt32(MaxFlightSeconds / TennisConst::SimDt);
	for (int32 Step = 0; Step < MaxSteps; ++Step)
	{
		Events.Reset();
		TennisBallPhysics::Step(State, TennisConst::SimDt, Context, Events);
		Flight.Apex = FMath::Max(Flight.Apex, State.Pos.Z);
		for (const FBallEvent& Event : Events)
		{
			const bool bNetEvent = Event.Type == EBallEventType::NetCross || Event.Type == EBallEventType::NetTouch;
			if (bNetEvent && !Flight.bReachedNet)
			{
				Flight.bReachedNet = true;
				Flight.NetClearance = Event.Pos.Z - Context.Ball.Radius - TennisCourtGeometry::NetHeightAt(Event.Pos.Y);
			}
			if (Event.Type == EBallEventType::Bounce)
			{
				Flight.bLanded = true;
				Flight.LandPos = Event.Pos;
				Flight.FlightTime = Event.Time - Launch.Time;
				return Flight;
			}
		}
	}
	Flight.LandPos = State.Pos;
	Flight.FlightTime = State.Time - Launch.Time;
	return Flight;
}

namespace
{
	// 残差 F = (飛距離, 条件) の (s, θ) についてのヤコビアン
	struct FJacobian
	{
		double RS = 0.0;  // dR/ds
		double RT = 0.0;  // dR/dθ
		double CS = 0.0;  // dC/ds
		double CT = 0.0;  // dC/dθ
	};

	// J·Δ = −F を解いた Δ = (Δs, Δθ)。行列が特異なら、飛距離だけを直す 1 変数の歩にする。何も直せなければ値なし
	TOptional<FVector2D> NewtonStep(const FJacobian& J, const FTrial& At, bool bFixedSpeed)
	{
		const double Det = J.RS * J.CT - J.RT * J.CS;
		if (FMath::Abs(Det) > UE_DOUBLE_SMALL_NUMBER)
		{
			return FVector2D(-(J.CT * At.RangeResidual - J.RT * At.ConstraintResidual) / Det,
				-(-J.CS * At.RangeResidual + J.RS * At.ConstraintResidual) / Det);
		}
		if (!bFixedSpeed && FMath::Abs(J.RS) > UE_DOUBLE_SMALL_NUMBER)
		{
			return FVector2D(-At.RangeResidual / J.RS, 0.0);  // 条件を測れない（ネットに届かない）ときは、速さで飛距離だけ直す
		}
		if (FMath::Abs(J.RT) > UE_DOUBLE_SMALL_NUMBER)
		{
			return FVector2D(0.0, -At.RangeResidual / J.RT);  // 速さを固定する球種は、仰角で飛距離を直す
		}
		return {};
	}

	// 1 つの条件の値で解いた結果（フォールバックの段階ごとに 1 回）
	struct FAttempt
	{
		FTrial Best;
		double Azimuth = 0.0;
		int32  Iterations = 0;
		bool   bConverged = false;
		bool   bSpeedAtMax = false;  // 速さの上限に当たった（余裕を大きくすれば解ける見込み）
	};

	FAttempt SolveWith(const FTennisShotSolver& Solver, const FShotRequest& Request, double ConstraintValue, const FBallSimContext& Ctx)
	{
		const FVector ToTarget(Request.Target.X - Request.Origin.X, Request.Target.Y - Request.Origin.Y, 0.0);
		const double Distance = ToTarget.Size();
		const bool bFixedSpeed = Request.Constraint == ESolverConstraint::FixedSpeed;
		const double SpeedLo = bFixedSpeed ? Request.Speed : Request.MinSpeed * MinSpeedFactor;
		const double SpeedHi = bFixedSpeed ? Request.Speed : Request.MaxSpeed * MaxSpeedFactor;

		FAttempt Attempt;
		Attempt.Azimuth = FMath::Atan2(ToTarget.Y, ToTarget.X);
		// 残差は目標への向き（固定）で測る。打ち出しの向きで測ると、方位を回しても曲がった分が残り続ける
		const FVector TargetDir = Heading(Attempt.Azimuth);
		const FVector TargetLeft(-TargetDir.Y, TargetDir.X, 0.0);
		// 1 回の試行（速さと仰角は範囲に収めてから飛ばす）
		const auto Evaluate = [&](double Speed, double Elevation)
		{
			FTrial Trial;
			Trial.Speed = FMath::Clamp(Speed, SpeedLo, SpeedHi);
			Trial.Elevation = FMath::Clamp(Elevation, MinElevation, MaxElevation);
			Trial.Flight = Solver.SimulateToFirstBounce(FTennisShotSolver::MakeLaunch(Request, Attempt.Azimuth, Trial.Speed, Trial.Elevation));
			const FVector Travel = Trial.Flight.LandPos - Request.Origin;
			Trial.RangeResidual = FVector::DotProduct(Travel, TargetDir) - Distance;
			Trial.LateralResidual = FVector::DotProduct(Travel, TargetLeft);
			switch (Request.Constraint)
			{
			case ESolverConstraint::NetClearance:
				// ネットに届かない短い球は、余裕を測れないので飛距離だけで直す
				Trial.ConstraintResidual = Trial.Flight.bReachedNet ? Trial.Flight.NetClearance - ConstraintValue : 0.0;
				break;
			case ESolverConstraint::Apex:
				Trial.ConstraintResidual = Trial.Flight.Apex - ConstraintValue;
				break;
			case ESolverConstraint::FixedSpeed:
				Trial.ConstraintResidual = 0.0;
				break;
			}
			return Trial;
		};
		// 片側差分のヤコビアン（試行を 1〜2 回使う）。範囲の端では内側へ取る（外へ取ると範囲に収められて、微分が 0 になる）
		const auto Differentiate = [&](const FTrial& At)
		{
			FJacobian J;
			const double DeltaT = At.Elevation + ElevationDelta <= MaxElevation ? ElevationDelta : -ElevationDelta;
			const FTrial ByElevation = Evaluate(At.Speed, At.Elevation + DeltaT);
			J.RT = (ByElevation.RangeResidual - At.RangeResidual) / DeltaT;
			J.CT = (ByElevation.ConstraintResidual - At.ConstraintResidual) / DeltaT;
			if (!bFixedSpeed)
			{
				const double DeltaS = At.Speed + SpeedDelta <= SpeedHi ? SpeedDelta : -SpeedDelta;
				const FTrial BySpeed = Evaluate(At.Speed + DeltaS, At.Elevation);
				J.RS = (BySpeed.RangeResidual - At.RangeResidual) / DeltaS;
				J.CS = (BySpeed.ConstraintResidual - At.ConstraintResidual) / DeltaS;
			}
			return J;
		};

		const TennisShotInitialGuess::FGuess Guess = TennisShotInitialGuess::Estimate(Request, Attempt.Azimuth, ConstraintValue, Ctx);
		double Speed = Guess.Speed;
		double Elevation = Guess.Elevation;

		bool bHasBest = false;
		for (int32 Pass = 0; Pass < MaxAzimuthPasses; ++Pass)
		{
			FTrial Current = Evaluate(Speed, Elevation);
			int32 IterationsAtSpeedLimit = 0;
			for (int32 Iteration = 0; Iteration < MaxNewtonIterations; ++Iteration)
			{
				++Attempt.Iterations;
				if (!bHasBest || Current.Score() < Attempt.Best.Score())
				{
					Attempt.Best = Current;
					bHasBest = true;
				}
				if (Current.IsConverged())
				{
					break;
				}
				// 速さが範囲の端に張り付いたままなら、この条件では解けない（次のフォールバックへ。時間を使い切らない）
				const bool bAtSpeedLimit = !bFixedSpeed
					&& (Current.Speed <= SpeedLo + UE_DOUBLE_KINDA_SMALL_NUMBER || Current.Speed >= SpeedHi - UE_DOUBLE_KINDA_SMALL_NUMBER);
				IterationsAtSpeedLimit = bAtSpeedLimit ? IterationsAtSpeedLimit + 1 : 0;
				if (IterationsAtSpeedLimit >= MaxIterationsAtSpeedLimit)
				{
					break;
				}

				const TOptional<FVector2D> Step = NewtonStep(Differentiate(Current), Current, bFixedSpeed);
				if (!Step.IsSet())
				{
					break;
				}
				// 全歩で試し、残差が悪くなれば歩幅を半分にして試し直す（後退）。最後まで悪ければ、いちばん小さい歩で進む
				FTrial Candidate;
				double Scale = 1.0;
				for (int32 Backtrack = 0; Backtrack <= MaxBacktracks; ++Backtrack, Scale *= 0.5)
				{
					Candidate = Evaluate(Current.Speed + Scale * Step->X, Current.Elevation + Scale * Step->Y);
					if (Candidate.Score() < Current.Score())
					{
						break;
					}
				}
				Current = Candidate;
			}
			Speed = Current.Speed;
			Elevation = Current.Elevation;
			if (!bHasBest || Current.Score() < Attempt.Best.Score())
			{
				Attempt.Best = Current;
				bHasBest = true;
			}

			// 方位の補正（外側のループ）：横のずれを打ち消す向きに回す
			if (!Current.IsConverged() || FMath::Abs(Current.LateralResidual) < LateralTolerance)
			{
				break;
			}
			Attempt.Azimuth -= FMath::Atan2(Current.LateralResidual, FMath::Max(Distance, 1.0));
			bHasBest = false;  // 方位を変えたので、前の方位の解は使わない
		}

		Attempt.bConverged = Attempt.Best.Flight.bLanded && Attempt.Best.IsConverged()
			&& FMath::Abs(Attempt.Best.LateralResidual) < LateralTolerance;
		Attempt.bSpeedAtMax = !bFixedSpeed && Attempt.Best.Speed >= SpeedHi - UE_DOUBLE_KINDA_SMALL_NUMBER;
		return Attempt;
	}
}

FShotSolveResult FTennisShotSolver::Solve(const FShotRequest& Request) const
{
	FAttempt Attempt = SolveWith(*this, Request, Request.ConstraintValue, Context);
	EShotFallback Fallback = EShotFallback::None;
	if (!Attempt.bConverged && Request.Constraint != ESolverConstraint::FixedSpeed)
	{
		// DD-02 §7.4：速さの上限に当たったら条件を大きく（遅く高く）、それ以外は条件を半分にして解き直す
		const bool bTooFast = Attempt.bSpeedAtMax;
		FAttempt Retry = SolveWith(*this, Request, Request.ConstraintValue * (bTooFast ? RelaxUpFactor : RelaxDownFactor), Context);
		Retry.Iterations += Attempt.Iterations;
		if (Retry.bConverged)
		{
			Fallback = bTooFast ? EShotFallback::SpeedClamped : EShotFallback::RelaxConstraint;
			Attempt = Retry;
		}
		else
		{
			Attempt.Iterations = Retry.Iterations;
			if (Retry.Best.Score() < Attempt.Best.Score())
			{
				Attempt = Retry;
			}
		}
	}

	const FTrial& Best = Attempt.Best;
	FShotSolveResult Result;
	Result.Launch = MakeLaunch(Request, Attempt.Azimuth, Best.Speed, Best.Elevation);
	Result.PredictedLanding = Best.Flight.LandPos;
	Result.ResidualRange = FVector::Dist2D(Best.Flight.LandPos, Request.Target);
	Result.ResidualConstraint = Best.ConstraintResidual;
	Result.PredictedNetClearance = Best.Flight.NetClearance;
	Result.PredictedFlightTime = Best.Flight.FlightTime;
	Result.Iterations = Attempt.Iterations;
	Result.bSolved = Attempt.bConverged;
	Result.Fallback = Attempt.bConverged ? Fallback : EShotFallback::Nearest;
	return Result;
}
