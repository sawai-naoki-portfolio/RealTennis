// W-1.3（後半）：1 ステップ（Step）と Bounce のイベント。T-BALL-001〜003（ドロップテスト。RES-02 §6、ITF Appendix I）。
// ドロップテストは校正の関数（Calibration/CourtCalibration.h）を使う。期待値は参照シミュレーションの drop_test。
#include "CQTest.h"
#include "Ball/TennisBallPhysics.h"
#include "Calibration/CourtCalibration.h"
#include "TennisConstants.h"

#include <limits>

namespace
{
	constexpr double Dt = TennisConst::SimDt;

	// ネット（X = 0）から離れたコートの上の位置。ネットの真上で落とすと白帯に当たる
	constexpr double AwayFromNetX = -8.0;  // m

	// ITF Appendix I：Type ごとの e_ball と空気密度で、ドロップテストの跳ね返り（cm）
	double DropTestReboundCm(double BallRestitution, double Rho)
	{
		FBallParams Ball;
		Ball.RestitutionHard = BallRestitution;
		FAirParams Air;
		Air.Rho = Rho;
		return TennisCourtCalibration::DropTestReboundCm(Ball, Air);
	}

	FBallSimContext NoAir()
	{
		FBallSimContext Ctx;
		Ctx.Air.Rho = 0.0;
		return Ctx;
	}
}

TEST_CLASS(BallStep, "Tennis.Sim.Ball")
{
	// T-BALL-001：Type 2（e_ball 0.775）は 135〜147 cm。参照 140.8 cm と ±1 cm
	TEST_METHOD(DropTest_Type2)
	{
		const double Rebound = DropTestReboundCm(0.775, 1.21);
		ASSERT_THAT(IsTrue(Rebound >= 135.0 && Rebound <= 147.0));
		ASSERT_THAT(IsNear(140.84, Rebound, 1.0));
	}

	// T-BALL-002：Type 1（e_ball 0.785）は 138〜151 cm。参照 144.4 cm と ±1 cm
	TEST_METHOD(DropTest_Type1)
	{
		const double Rebound = DropTestReboundCm(0.785, 1.21);
		ASSERT_THAT(IsTrue(Rebound >= 138.0 && Rebound <= 151.0));
		ASSERT_THAT(IsNear(144.39, Rebound, 1.0));
	}

	// T-BALL-003：高地球（e_ball 0.740、ρ = 1.058）は 122〜135 cm。参照 130.1 cm と ±1 cm
	TEST_METHOD(DropTest_HighAltitude)
	{
		const double Rebound = DropTestReboundCm(0.740, 1.058);
		ASSERT_THAT(IsTrue(Rebound >= 122.0 && Rebound <= 135.0));
		ASSERT_THAT(IsNear(130.07, Rebound, 1.0));
	}

	// ACT-01：接地の時刻はステップの中で補間する（空気なしの解析解 t = √(2h/g) と 0.1 ms 以内）
	TEST_METHOD(Bounce_IsTimedWithinTheStep)
	{
		const FBallSimContext Ctx = NoAir();
		FBallState State;
		State.Pos = FVector(AwayFromNetX, 0.0, 1.0 + Ctx.Ball.Radius);
		TArray<FBallEvent> Events;
		int32 Steps = 0;
		while (Events.IsEmpty() && Steps < 240)
		{
			TennisBallPhysics::Step(State, Dt, Ctx, Events);
			++Steps;
		}
		ASSERT_THAT(AreEqual(1, Events.Num()));
		const FBallEvent& Bounce = Events[0];
		ASSERT_THAT(IsTrue(Bounce.Type == EBallEventType::Bounce));
		ASSERT_THAT(IsNear(FMath::Sqrt(2.0 / TennisConst::Gravity), Bounce.Time, 1e-4));
		ASSERT_THAT(IsNear(FMath::Sqrt(2.0 * TennisConst::Gravity), Bounce.NormalSpeed, 1e-3));
		ASSERT_THAT(IsNear(Ctx.Ball.Radius, Bounce.Pos.Z, 1e-12));
		ASSERT_THAT(IsTrue(Bounce.Vel.Z > 0.0));
		ASSERT_THAT(IsNear(Ctx.Surface.ContactPatch(Bounce.NormalSpeed), Bounce.ContactPatch, 1e-12));
		ASSERT_THAT(IsNear(Steps * Dt, State.Time, 1e-12));  // ステップの時刻は Dt の倍数のまま
		ASSERT_THAT(AreEqual(1, State.NumBounces));
	}

	// D-7：地面に届かないステップは、IntegrateFlight と完全に同じ
	TEST_METHOD(StepInAir_EqualsIntegrateFlight)
	{
		const FBallSimContext Ctx;
		FBallState A;
		A.Pos = FVector(-11.0, 0.0, 1.0);
		A.Vel = FVector(30.0, 1.0, 3.0);
		A.Spin = FVector(0.0, 250.0, 0.0);
		FBallState B = A;
		TArray<FBallEvent> Events;
		for (int32 Step = 0; Step < 60; ++Step)
		{
			TennisBallPhysics::Step(A, Dt, Ctx, Events);
			TennisBallPhysics::IntegrateFlight(B, Dt, Ctx);
		}
		ASSERT_THAT(IsTrue(Events.IsEmpty()));
		ASSERT_THAT(IsTrue(A.Pos == B.Pos && A.Vel == B.Vel && A.Spin == B.Spin && A.Time == B.Time));
	}

	// 堅牢性：弾み続けた球は、やがて地面に沿って止まり、地面の下に入らない。イベントは時刻の順
	TEST_METHOD(RepeatedBounces_SettleOnTheGround)
	{
		const FBallSimContext Ctx;
		FBallState State;
		State.Pos = FVector(AwayFromNetX, 0.0, 1.0 + Ctx.Ball.Radius);
		TArray<FBallEvent> Events;
		for (int32 Step = 0; Step < 10 * 240; ++Step)
		{
			TennisBallPhysics::Step(State, Dt, Ctx, Events);
			ASSERT_THAT(IsTrue(State.Pos.Z >= Ctx.Ball.Radius - 1e-12));
		}
		ASSERT_THAT(IsTrue(Events.Num() > 3 && Events.Num() < 100));
		for (int32 Index = 1; Index < Events.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(Events[Index - 1].Time < Events[Index].Time));
		}
		ASSERT_THAT(IsNear(Ctx.Ball.Radius, State.Pos.Z, 1e-12));
		ASSERT_THAT(IsNear(0.0, State.Vel.Z, 1e-12));
		ASSERT_THAT(AreEqual(Events.Num(), State.NumBounces));
	}

	// 堅牢性：地面の下から始まった球は、ステップの始めで弾み、地面の上に戻る
	TEST_METHOD(BallStartingBelowGround_BouncesAtStepStart)
	{
		const FBallSimContext Ctx;
		FBallState State;
		State.Pos = FVector(AwayFromNetX, 0.0, Ctx.Ball.Radius - 0.01);
		State.Vel = FVector(5.0, 0.0, -3.0);
		TArray<FBallEvent> Events;
		TennisBallPhysics::Step(State, Dt, Ctx, Events);
		ASSERT_THAT(AreEqual(1, Events.Num()));
		ASSERT_THAT(IsNear(0.0, Events[0].Time, 1e-12));
		ASSERT_THAT(IsTrue(State.Pos.Z > Ctx.Ball.Radius && State.Vel.Z > 0.0));
	}

	// OutEvents は後ろに加えるだけ（呼び出し側のイベントを消さない）。不正な Dt は何も変えない
	TEST_METHOD(EventsAreAppended_InvalidStepChangesNothing)
	{
		const FBallSimContext Ctx;
		FBallState State;
		State.Pos = FVector(AwayFromNetX, 0.0, Ctx.Ball.Radius + 0.001);
		State.Vel = FVector(0.0, 0.0, -5.0);
		TArray<FBallEvent> Events;
		Events.AddDefaulted();
		TennisBallPhysics::Step(State, Dt, Ctx, Events);
		ASSERT_THAT(AreEqual(2, Events.Num()));

		const FBallState Before = State;
		for (const double Bad : { -Dt, std::numeric_limits<double>::quiet_NaN() })
		{
			TennisBallPhysics::Step(State, Bad, Ctx, Events);
		}
		ASSERT_THAT(AreEqual(2, Events.Num()));
		ASSERT_THAT(IsTrue(State.Pos == Before.Pos && State.Vel == Before.Vel && State.Time == Before.Time));
	}
};
