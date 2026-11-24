// W-2.1：ショットソルバ（DD-02 §7・§10）。
#include "CQTest.h"
#include "Court/CourtGeometry.h"
#include "Math/RandomStream.h"
#include "Shot/TennisShotSolver.h"
#include "TennisUnits.h"
#include "TennisTestHelpers.h"

namespace
{
	// DD-02 §2 のグラウンドストロークの球種（回転・速さの範囲・余裕の溜め 0〜1・初期仰角）
	struct FStrokeType { const TCHAR* Name; double Rpm, MinSpeed, MaxSpeed, Clearance0, Clearance1, ElevationDeg; };
	constexpr FStrokeType StrokeTypes[] = {
		{ TEXT("Flat"),          600.0, 25.0, 46.0, 0.70, 0.25,  5.0 },
		{ TEXT("Topspin"),      2300.0, 22.0, 38.0, 1.40, 0.60,  9.0 },
		{ TEXT("HeavyTopspin"), 3200.0, 22.0, 36.0, 1.80, 0.90, 12.0 },
		{ TEXT("Slice"),       -1500.0, 18.0, 31.0, 0.60, 0.25,  4.0 },
	};

	FShotRequest RandomRallyShot(FRandomStream& Stream)
	{
		const FStrokeType& Type = StrokeTypes[Stream.RandRange(0, UE_ARRAY_COUNT(StrokeTypes) - 1)];
		FShotRequest Request;
		Request.Origin = FVector(Stream.FRandRange(-12.5, -9.0), Stream.FRandRange(-4.0, 4.0), Stream.FRandRange(0.6, 1.4));
		Request.Target = FVector(Stream.FRandRange(TennisConst::ServiceLineX, 11.5), Stream.FRandRange(-3.8, 3.8), 0.0);  // グラウンドストロークの深さ
		Request.Constraint = ESolverConstraint::NetClearance;
		Request.ConstraintValue = FMath::Lerp(Type.Clearance0, Type.Clearance1, Stream.GetFraction());  // 溜め 0〜1
		Request.MinSpeed = Type.MinSpeed;
		Request.MaxSpeed = Type.MaxSpeed;
		Request.InitialElevationDeg = Type.ElevationDeg;
		Request.Rpm = Type.Rpm;
		return Request;
	}

	// 解いた打ち出しを、本番と同じ Step で飛ばした最初のバウンドの位置（D-7：ソルバの試行と同じになるはず）
	FVector ActualLanding(const FShotSolveResult& Result)
	{
		FBallState State = Result.Launch;
		const TArray<FBallEvent> Events = TennisTest::FlyToFirstBounce(State, FBallSimContext(), 4.0);
		const FBallEvent* Bounce = TennisTest::FirstOf(Events, EBallEventType::Bounce);
		return Bounce != nullptr ? Bounce->Pos : FVector(TNumericLimits<double>::Max());
	}
}

TEST_CLASS(ShotSolver, "Tennis.Sim.Shot")
{
	// T-SHOT-001：ランダムな打点・目標・球種の 1,000 件で、95% 以上が 5 cm 以内に着地する（本番と同じ物理で飛ばして確かめる）
	// T-SHOT-002：1 打の計算時間の P99 < 1.0 ms
	TEST_METHOD(RandomRallyShots_LandWithin5cm_Fast)
	{
		const FTennisShotSolver Solver{ FBallSimContext() };
		FRandomStream Stream(2026);
		constexpr int32 NumCases = 1000;
		int32 WithinTolerance = 0;
		TArray<double> Milliseconds;
		Milliseconds.Reserve(NumCases);
		for (int32 Case = 0; Case < NumCases; ++Case)
		{
			const FShotRequest Request = RandomRallyShot(Stream);
			const double Start = FPlatformTime::Seconds();
			const FShotSolveResult Result = Solver.Solve(Request);
			Milliseconds.Add((FPlatformTime::Seconds() - Start) * 1000.0);
			const FVector Landing = ActualLanding(Result);
			if (Result.bSolved && FVector::Dist2D(Landing, Request.Target) < 0.05 && Landing.Equals(Result.PredictedLanding, 1e-9))
			{
				++WithinTolerance;
			}
		}
		Milliseconds.Sort();
		const double P99 = Milliseconds[FMath::FloorToInt32(0.99 * (NumCases - 1))];
		UE_LOG(LogTemp, Display, TEXT("[ShotSolver] within 5 cm: %d / %d, P50 %.3f ms, P90 %.3f ms, P99 %.3f ms, max %.3f ms"),
			WithinTolerance, NumCases, Milliseconds[NumCases / 2], Milliseconds[FMath::FloorToInt32(0.9 * (NumCases - 1))], P99, Milliseconds.Last());
		ASSERT_THAT(IsTrue(WithinTolerance >= 950));
		ASSERT_THAT(IsTrue(P99 < 1.0));
	}

	// 速さを固定する球種（サーブ）は θ だけを解く。横に曲がる回転（スライスの軸 +65°）も、方位の補正で目標に落ちる
	TEST_METHOD(FixedSpeedServes_WithSidespin_LandOnTarget)
	{
		const FTennisShotSolver Solver{ FBallSimContext() };
		for (const double AxisDeg : { 10.0, 65.0, -20.0 })
		{
			FShotRequest Request;
			Request.Origin = FVector(-11.985, 0.3, 2.9);
			Request.Target = FVector(5.8, -2.6, 0.0);
			Request.Constraint = ESolverConstraint::FixedSpeed;
			Request.Speed = 48.0;
			Request.InitialElevationDeg = -5.0;
			Request.Rpm = 2200.0;
			Request.SpinAxisDeg = AxisDeg;
			const FShotSolveResult Result = Solver.Solve(Request);
			ASSERT_THAT(IsTrue(Result.bSolved));
			ASSERT_THAT(IsTrue(FVector::Dist2D(ActualLanding(Result), Request.Target) < 0.05));
			ASSERT_THAT(IsNear(48.0, Result.Launch.Vel.Size(), 1e-9));
			ASSERT_THAT(IsTrue(Result.PredictedNetClearance > 0.0));
		}
	}

	// 最高点を条件にする球種（ロブ）
	TEST_METHOD(Lob_ReachesItsApexAndLandsOnTarget)
	{
		const FTennisShotSolver Solver{ FBallSimContext() };
		FShotRequest Request;
		Request.Origin = FVector(-11.0, 1.0, 1.0);
		Request.Target = FVector(9.5, -1.0, 0.0);
		Request.Constraint = ESolverConstraint::Apex;
		Request.ConstraintValue = 8.0;
		Request.MinSpeed = 15.0;
		Request.MaxSpeed = 26.0;
		Request.InitialElevationDeg = 30.0;
		Request.Rpm = 1500.0;
		const FShotSolveResult Result = Solver.Solve(Request);
		ASSERT_THAT(IsTrue(Result.bSolved));
		ASSERT_THAT(IsTrue(FVector::Dist2D(ActualLanding(Result), Request.Target) < 0.05));
		ASSERT_THAT(IsTrue(FMath::Abs(Result.ResidualConstraint) < 0.05));
	}

	// T-SHOT-004：ネットの 0.5 m 手前・高さ 0.2 m の打点からフラットで深く → 解けず、Fallback = Nearest。目標から大きく外れる（ネットかアウト）
	TEST_METHOD(FlatFromLowPointNearNetDeep_FallsBackToNearest)
	{
		const FTennisShotSolver Solver{ FBallSimContext() };
		FShotRequest Request;
		Request.Origin = FVector(-0.5, 0.0, 0.2);
		Request.Target = FVector(10.5, 0.0, 0.0);
		Request.ConstraintValue = 0.25;  // Flat の溜め 1
		Request.MinSpeed = 25.0;
		Request.MaxSpeed = 46.0;
		Request.InitialElevationDeg = 5.0;
		Request.Rpm = 600.0;
		const FShotSolveResult Result = Solver.Solve(Request);
		ASSERT_THAT(IsFalse(Result.bSolved));
		ASSERT_THAT(IsTrue(Result.Fallback == EShotFallback::Nearest));
		ASSERT_THAT(IsTrue(Result.ResidualRange > 1.0));
	}

	// 同じリクエストなら、同じ打ち出し（決定論）
	TEST_METHOD(SameRequest_GivesSameLaunch)
	{
		const FTennisShotSolver Solver{ FBallSimContext() };
		FRandomStream Stream(7);
		const FShotRequest Request = RandomRallyShot(Stream);
		const FShotSolveResult A = Solver.Solve(Request);
		const FShotSolveResult B = Solver.Solve(Request);
		ASSERT_THAT(IsTrue(A.Launch.Vel == B.Launch.Vel && A.Launch.Spin == B.Launch.Spin && A.Iterations == B.Iterations));
	}

	// ソルバの 1 回の試行は、本番と同じ Step で飛ばした結果と同じ。ネットの上の余裕は、ネットの面を越える瞬間の Z − R − h(Y)
	TEST_METHOD(SimulateToFirstBounce_MatchesProductionAndMeasuresClearance)
	{
		const FTennisShotSolver Solver{ FBallSimContext() };
		FShotRequest Request;
		Request.Origin = FVector(-11.0, 0.5, 1.0);
		Request.Rpm = 2300.0;
		const FBallState Launch = FTennisShotSolver::MakeLaunch(Request, 0.0, 30.0, FMath::DegreesToRadians(8.0));
		const FTennisShotSolver::FFlight Flight = Solver.SimulateToFirstBounce(Launch);

		FBallState State = Launch;
		const TArray<FBallEvent> Events = TennisTest::FlyToFirstBounce(State, FBallSimContext(), 4.0);
		const FBallEvent* Bounce = TennisTest::FirstOf(Events, EBallEventType::Bounce);
		const FBallEvent* Cross = TennisTest::FirstOf(Events, EBallEventType::NetCross);
		ASSERT_THAT(IsTrue(Bounce != nullptr && Cross != nullptr));
		ASSERT_THAT(IsTrue(Flight.bLanded && Flight.LandPos.Equals(Bounce->Pos, 1e-9) && Flight.LandPos.X > 0.0));
		ASSERT_THAT(IsNear(Bounce->Time - Launch.Time, Flight.FlightTime, 1e-9));
		ASSERT_THAT(IsTrue(Flight.bReachedNet));
		ASSERT_THAT(IsNear(Cross->Pos.Z - FBallParams().Radius - TennisCourtGeometry::NetHeightAt(Cross->Pos.Y), Flight.NetClearance, 1e-9));
		ASSERT_THAT(IsTrue(Flight.Apex > Request.Origin.Z));
	}

	// 最初のバウンドがなければ（真上に打った球が 4 s では落ちない）、最後の位置を返す
	TEST_METHOD(SimulateToFirstBounce_WithoutBounce_ReturnsLastState)
	{
		const FTennisShotSolver Solver{ FBallSimContext() };
		FShotRequest Request;
		Request.Origin = FVector(-11.0, 0.0, 1.0);
		const FBallState Launch = FTennisShotSolver::MakeLaunch(Request, 0.0, 45.0, FMath::DegreesToRadians(89.0));
		const FTennisShotSolver::FFlight Flight = Solver.SimulateToFirstBounce(Launch);
		ASSERT_THAT(IsFalse(Flight.bLanded));
		ASSERT_THAT(IsFalse(Flight.bReachedNet));
		ASSERT_THAT(IsTrue(Flight.LandPos.Z > FBallParams().Radius));
		ASSERT_THAT(IsNear(4.0, Flight.FlightTime, TennisConst::SimDt));
	}

	// DD-02 §3.1：+X へ打つトップスピンは +Y 軸まわり。φ > 0 は上向きの成分（左へ曲がる）を持つ
	TEST_METHOD(MakeLaunch_BuildsSpinFromRpmAndAxis)
	{
		FShotRequest Request;
		Request.Rpm = 100.0 / TennisUnits::RpmToRadPerSec(1.0);  // 100 rad/s
		const FBallState Top = FTennisShotSolver::MakeLaunch(Request, 0.0, 30.0, 0.0);
		ASSERT_THAT(IsTrue(Top.Spin.Equals(FVector(0.0, 100.0, 0.0), 1e-9)));
		Request.SpinAxisDeg = 90.0;
		const FBallState Side = FTennisShotSolver::MakeLaunch(Request, 0.0, 30.0, 0.0);
		ASSERT_THAT(IsTrue(Side.Spin.Equals(FVector(0.0, 0.0, 100.0), 1e-9)));
	}
};
