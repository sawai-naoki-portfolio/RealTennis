// W-2.3：サーブの回転の軸。T-SHOT-005（DD-02 §3.1・§10）。
#include "CQTest.h"
#include "Shot/ShotSpin.h"
#include "Shot/TennisShotSolver.h"

namespace
{
	// DD-02 §2 のサーブの球種（溜めの途中の速さ、回転数、右利きの回転の軸、初期の仰角）
	struct FServeType { double Speed, Rpm, AxisDeg, ElevationDeg; };
	constexpr FServeType ServeFlat{ 55.0, 1200.0, 10.0, -5.0 };
	constexpr FServeType ServeSlice{ 47.0, 2200.0, 65.0, -3.0 };
	constexpr FServeType ServeKick{ 40.0, 3200.0, -20.0, 0.0 };

	struct FServeCurve
	{
		bool   bSolved = false;
		double Curve = 0.0;  // m（打ち出しの向きに対する着地点の左右。左が正）
	};

	// デュースサイドからセンターへ打つサーブを解き、横のずれを返す
	FServeCurve SolveServe(const FServeType& Type, ETennisHandedness Handedness, double SideInput = 0.0)
	{
		const FTennisShotSolver Solver{ FBallSimContext() };
		FShotRequest Request;
		Request.Origin = FVector(-11.985, -0.5, 2.9);
		Request.Target = FVector(5.5, 0.5, 0.0);
		Request.Constraint = ESolverConstraint::FixedSpeed;
		Request.Speed = Type.Speed;
		Request.InitialElevationDeg = Type.ElevationDeg;
		Request.Rpm = Type.Rpm;
		Request.SpinAxisDeg = TennisShotSpin::AxisDeg(Type.AxisDeg, Handedness, SideInput);
		const FShotSolveResult Result = Solver.Solve(Request);

		const FVector Heading = Result.Launch.Vel.GetSafeNormal2D();
		const FVector Travel = Result.PredictedLanding - Request.Origin;
		return FServeCurve{ Result.bSolved, Heading.X * Travel.Y - Heading.Y * Travel.X };  // (H × 移動).Z
	}
}

TEST_CLASS(ShotSpin, "Tennis.Sim.Shot")
{
	// 左利きは符号を反転する。横の入力は右へ倒すほど φ を減らし（右へ曲がる）、−1〜1 に収める。数でない入力は 0
	TEST_METHOD(AxisDeg_FlipsForLeftHandersAndTiltsWithSideInput)
	{
		ASSERT_THAT(IsNear(65.0, TennisShotSpin::AxisDeg(65.0, ETennisHandedness::Right, 0.0), 1e-12));
		ASSERT_THAT(IsNear(-65.0, TennisShotSpin::AxisDeg(65.0, ETennisHandedness::Left, 0.0), 1e-12));
		ASSERT_THAT(IsNear(50.0, TennisShotSpin::AxisDeg(65.0, ETennisHandedness::Right, 1.0), 1e-12));
		ASSERT_THAT(IsNear(80.0, TennisShotSpin::AxisDeg(65.0, ETennisHandedness::Right, -1.0), 1e-12));
		ASSERT_THAT(IsNear(-80.0, TennisShotSpin::AxisDeg(65.0, ETennisHandedness::Left, 1.0), 1e-12));
		ASSERT_THAT(IsNear(50.0, TennisShotSpin::AxisDeg(65.0, ETennisHandedness::Right, 3.0), 1e-12));
		ASSERT_THAT(IsNear(65.0, TennisShotSpin::AxisDeg(65.0, ETennisHandedness::Right, std::numeric_limits<double>::quiet_NaN()), 1e-12));
	}

	// T-SHOT-005：右利きのフラットとスライスは左に、キックは右に曲がる。スライスがいちばん曲がる。左利きは逆
	TEST_METHOD(Serves_CurveByTypeAndHandedness)
	{
		for (const ETennisHandedness Handedness : { ETennisHandedness::Right, ETennisHandedness::Left })
		{
			const double Sign = Handedness == ETennisHandedness::Right ? 1.0 : -1.0;
			const FServeCurve Flat = SolveServe(ServeFlat, Handedness);
			const FServeCurve Slice = SolveServe(ServeSlice, Handedness);
			const FServeCurve Kick = SolveServe(ServeKick, Handedness);
			ASSERT_THAT(IsTrue(Flat.bSolved && Slice.bSolved && Kick.bSolved));
			ASSERT_THAT(IsTrue(Sign * Flat.Curve > 0.0));
			ASSERT_THAT(IsTrue(Sign * Slice.Curve > Sign * Flat.Curve));
			ASSERT_THAT(IsTrue(Sign * Kick.Curve < 0.0));
		}
	}

	// 横の入力でスライスの曲がりを強められる（右利きは左へ倒す）
	TEST_METHOD(SliceServe_CurvesMoreWhenTiltedTowardItsCurve)
	{
		const FServeCurve Neutral = SolveServe(ServeSlice, ETennisHandedness::Right);
		const FServeCurve Tilted = SolveServe(ServeSlice, ETennisHandedness::Right, -1.0);
		ASSERT_THAT(IsTrue(Neutral.bSolved && Tilted.bSolved));
		ASSERT_THAT(IsTrue(Tilted.Curve > Neutral.Curve));
	}
};
