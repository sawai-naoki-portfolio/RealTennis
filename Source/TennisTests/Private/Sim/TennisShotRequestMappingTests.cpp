// W-2.5：球種のデータからソルバのリクエストへの写し方（DD-02 §4.1）。
#include "CQTest.h"
#include "Shot/ShotRequestMapping.h"
#include "Shot/TennisShotSolver.h"

#include <limits>

namespace
{
	// DD-02 §2 の Topspin と ServeKick
	FShotTypeParams Topspin()
	{
		FShotTypeParams Params;
		Params.MinSpeed = 22.0;
		Params.MaxSpeed = 38.0;
		Params.BaseRpm = 2300.0;
		Params.ConstraintAtCharge0 = 1.40;
		Params.ConstraintAtCharge1 = 0.60;
		Params.InitialElevationDeg = 9.0;
		return Params;
	}

	FShotTypeParams ServeKick()
	{
		FShotTypeParams Params;
		Params.MinSpeed = 35.0;
		Params.MaxSpeed = 46.0;
		Params.BaseRpm = 3200.0;
		Params.SpinAxisDeg = -20.0;
		Params.Constraint = ESolverConstraint::FixedSpeed;
		Params.InitialElevationDeg = 0.0;
		return Params;
	}

	FShotRequest MakeAt(const FShotTypeParams& Params, double Charge, ETennisHandedness Handedness = ETennisHandedness::Right)
	{
		return TennisShotRequestMapping::Make(Params, FVector(-11.0, 0.5, 1.0), FVector(9.0, -2.0, 0.0), 3.25, Charge, Handedness, 0.0);
	}
}

TEST_CLASS(ShotRequestMapping, "Tennis.Sim.Shot")
{
	// 打点・狙い・時刻・速さの範囲・初期の仰角・回転はそのまま。条件の目標は溜めで補間する
	TEST_METHOD(RallyShot_CopiesParamsAndInterpolatesConstraintByCharge)
	{
		const FShotRequest Request = MakeAt(Topspin(), 0.25);
		ASSERT_THAT(IsTrue(Request.Origin == FVector(-11.0, 0.5, 1.0) && Request.Target == FVector(9.0, -2.0, 0.0)));
		ASSERT_THAT(IsNear(3.25, Request.Time, 1e-12));
		ASSERT_THAT(IsTrue(Request.Constraint == ESolverConstraint::NetClearance));
		ASSERT_THAT(IsNear(1.40 + 0.25 * (0.60 - 1.40), Request.ConstraintValue, 1e-12));
		ASSERT_THAT(IsNear(22.0, Request.MinSpeed, 1e-12));
		ASSERT_THAT(IsNear(38.0, Request.MaxSpeed, 1e-12));
		ASSERT_THAT(IsNear(9.0, Request.InitialElevationDeg, 1e-12));
		ASSERT_THAT(IsNear(2300.0, Request.Rpm, 1e-12));
		ASSERT_THAT(IsNear(0.0, Request.SpinAxisDeg, 1e-12));
	}

	// 速さを固定する球種は、s = lerp(Min, Max, Charge^0.8)。回転の軸は利き手で反転する
	TEST_METHOD(FixedSpeedShot_TakesSpeedFromChargeCurve)
	{
		ASSERT_THAT(IsNear(35.0, MakeAt(ServeKick(), 0.0).Speed, 1e-12));
		ASSERT_THAT(IsNear(46.0, MakeAt(ServeKick(), 1.0).Speed, 1e-12));
		ASSERT_THAT(IsNear(35.0 + 11.0 * FMath::Pow(0.5, 0.8), MakeAt(ServeKick(), 0.5).Speed, 1e-12));
		ASSERT_THAT(IsNear(-20.0, MakeAt(ServeKick(), 0.5).SpinAxisDeg, 1e-12));
		ASSERT_THAT(IsNear(20.0, MakeAt(ServeKick(), 0.5, ETennisHandedness::Left).SpinAxisDeg, 1e-12));
	}

	// 境界：溜めは 0〜1 に収め、数でない値は 0（警告のログ）
	TEST_METHOD(Charge_IsClampedAndNaNIsZero)
	{
		ASSERT_THAT(IsNear(1.40, MakeAt(Topspin(), -0.5).ConstraintValue, 1e-12));
		ASSERT_THAT(IsNear(0.60, MakeAt(Topspin(), 2.0).ConstraintValue, 1e-12));
		ASSERT_THAT(IsNear(1.40, MakeAt(Topspin(), std::numeric_limits<double>::quiet_NaN()).ConstraintValue, 1e-12));
		ASSERT_THAT(IsNear(46.0, MakeAt(ServeKick(), 2.0).Speed, 1e-12));
	}

	// 写したリクエストは、そのままソルバで解ける
	TEST_METHOD(MappedRequest_IsSolvable)
	{
		const FTennisShotSolver Solver{ FBallSimContext() };
		const FShotSolveResult Rally = Solver.Solve(MakeAt(Topspin(), 0.5));
		ASSERT_THAT(IsTrue(Rally.bSolved && Rally.Fallback == EShotFallback::None));
		const FShotSolveResult Serve = Solver.Solve(TennisShotRequestMapping::Make(
			ServeKick(), FVector(-11.985, -0.5, 2.8), FVector(5.0, 1.5, 0.0), 0.0, 0.7, ETennisHandedness::Right, 0.0));
		ASSERT_THAT(IsTrue(Serve.bSolved));
	}

	// T-STRK-016：インパクトでの溜め（DD-03 §6.4 の 6）。離していれば ChargeTime の比を 0〜1 に、押したまま・自動で離したら 1
	TEST_METHOD(ChargeAtImpact_IsTheHeldRatioOrFullWhenStillHeld)
	{
		FShotIntent Intent;
		Intent.ChargeStartTime = 10.0;
		Intent.ReleaseTime = 10.35;
		ASSERT_THAT(IsNear(0.5, TennisShotRequestMapping::ChargeAtImpact(Intent, 0.70), 1e-12));
		Intent.ReleaseTime = 11.0;
		ASSERT_THAT(IsTrue(TennisShotRequestMapping::ChargeAtImpact(Intent, 0.70) == 1.0));
		Intent.ReleaseTime = 9.9;  // 時計が戻った：0 に収める
		ASSERT_THAT(IsTrue(TennisShotRequestMapping::ChargeAtImpact(Intent, 0.70) == 0.0));

		FShotIntent Held;
		Held.ChargeStartTime = 10.0;  // 離していない：入力がインパクトで自動で離す（溜めは最大）
		ASSERT_THAT(IsTrue(TennisShotRequestMapping::ChargeAtImpact(Held, 0.70) == 1.0));
		FShotIntent Auto = Intent;
		Auto.bAutoReleased = true;
		ASSERT_THAT(IsTrue(TennisShotRequestMapping::ChargeAtImpact(Auto, 0.70) == 1.0));
	}

	// 溜めの始まりがない・ChargeTime が正でない・数でないときは、警告を出して 1
	TEST_METHOD(ChargeAtImpact_IsFullWhenTheInputCannotBeRead)
	{
		FShotIntent Intent;
		Intent.ChargeStartTime = 10.0;
		Intent.ReleaseTime = 10.35;
		ASSERT_THAT(IsTrue(TennisShotRequestMapping::ChargeAtImpact(Intent, 0.0) == 1.0));
		ASSERT_THAT(IsTrue(TennisShotRequestMapping::ChargeAtImpact(Intent, -0.7) == 1.0));
		ASSERT_THAT(IsTrue(TennisShotRequestMapping::ChargeAtImpact(Intent, std::numeric_limits<double>::quiet_NaN()) == 1.0));
		ASSERT_THAT(IsTrue(TennisShotRequestMapping::ChargeAtImpact(Intent, std::numeric_limits<double>::infinity()) == 1.0));  // 比が 0 にならない
		FShotIntent NoStart;
		NoStart.ReleaseTime = 10.35;
		ASSERT_THAT(IsTrue(TennisShotRequestMapping::ChargeAtImpact(NoStart, 0.70) == 1.0));
		FShotIntent BadTime = Intent;
		BadTime.ReleaseTime = std::numeric_limits<double>::infinity();
		ASSERT_THAT(IsTrue(TennisShotRequestMapping::ChargeAtImpact(BadTime, 0.70) == 1.0));
		FShotIntent BadStart = Intent;
		BadStart.ChargeStartTime = std::numeric_limits<double>::quiet_NaN();
		ASSERT_THAT(IsTrue(TennisShotRequestMapping::ChargeAtImpact(BadStart, 0.70) == 1.0));
	}
};
