// W-1.1：ボールのシミュレーションの基本の型。既定値が DATA-02 の初期値と ITF の規格に合うこと、反発係数の式と境界。
#include "CQTest.h"
#include "TennisSimTypes.h"

namespace
{
	constexpr double Exact = 1e-12;
}

TEST_CLASS(TennisSimTypes, "Tennis.Sim.Types")
{
	TEST_METHOD(BallState_StartsAtRestWithNoHitter)
	{
		const FBallState State;
		ASSERT_THAT(IsTrue(State.Pos.IsZero() && State.Vel.IsZero() && State.Spin.IsZero()));
		ASSERT_THAT(AreEqual(0, State.NumBounces));
		ASSERT_THAT(AreEqual(INDEX_NONE, State.LastHitter));
		ASSERT_THAT(IsFalse(State.bNetTouched));
		ASSERT_THAT(IsFalse(State.bInPlay));
	}

	TEST_METHOD(BallParams_DefaultIsItfType2Ball)
	{
		// ITF Appendix I：質量 56.0〜59.4 g、Type 2 の直径 6.54〜6.86 cm
		const FBallParams Ball;
		ASSERT_THAT(IsTrue(Ball.Mass >= 0.0560 && Ball.Mass <= 0.0594));
		ASSERT_THAT(IsTrue(2.0 * Ball.Radius >= 0.0654 && 2.0 * Ball.Radius <= 0.0686));
	}

	TEST_METHOD(BallParams_DerivedQuantities)
	{
		const FBallParams Ball;
		ASSERT_THAT(IsNear(UE_DOUBLE_PI * 0.0335 * 0.0335, Ball.CrossSectionArea(), Exact));
		ASSERT_THAT(IsNear(0.55 * 0.0577 * 0.0335 * 0.0335, Ball.MomentOfInertia(), Exact));
	}

	TEST_METHOD(Surface_RestitutionIsReferenceAtReferenceConditions)
	{
		const FSurfaceParams Surface;
		ASSERT_THAT(IsNear(Surface.RestitutionRef,
			Surface.RestitutionAt(FSurfaceParams::ReferenceNormalSpeed, FSurfaceParams::ReferenceTempC), Exact));
	}

	TEST_METHOD(Surface_RestitutionFallsWithSpeedAndRisesWithTemperature)
	{
		const FSurfaceParams Surface;
		const double Ref = Surface.RestitutionAt(8.27, 23.0);
		ASSERT_THAT(IsNear(Ref - 0.008 * 10.0, Surface.RestitutionAt(18.27, 23.0), 1e-9));
		ASSERT_THAT(IsNear(Ref + 0.003 * 10.0, Surface.RestitutionAt(8.27, 33.0), 1e-9));
	}

	TEST_METHOD(Surface_RestitutionIsClampedToPhysicalRange)
	{
		const FSurfaceParams Surface;
		ASSERT_THAT(IsNear(FSurfaceParams::MinRestitution, Surface.RestitutionAt(200.0, 23.0), Exact));
		ASSERT_THAT(IsNear(FSurfaceParams::MaxRestitution, Surface.RestitutionAt(0.0, 60.0), Exact));
	}

	TEST_METHOD(Surface_ContactPatchGrowsWithSpeedAndIgnoresNegativeInput)
	{
		const FSurfaceParams Surface;
		ASSERT_THAT(IsNear(0.012, Surface.ContactPatch(0.0), Exact));
		ASSERT_THAT(IsNear(0.012 + 0.0008 * 20.0, Surface.ContactPatch(20.0), Exact));
		ASSERT_THAT(IsNear(Surface.ContactPatch(0.0), Surface.ContactPatch(-5.0), Exact));
	}

	TEST_METHOD(Context_DefaultsMatchParameterTable)
	{
		const FBallSimContext Ctx;
		ASSERT_THAT(IsNear(1.204, Ctx.Air.Rho, Exact));
		ASSERT_THAT(IsTrue(Ctx.Air.WindMean.IsZero()));
		ASSERT_THAT(IsNear(20.0, Ctx.TempC, Exact));
	}
};
