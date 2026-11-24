// W-1.2：空中のボール（加速度・RK4・回転の減衰）。T-BALL-005〜008・011（RES-02 §6）。
// 期待値は参照シミュレーション Tools/Reference/ballsim_reference.py と同じモデル・同じ条件の値。
#include "CQTest.h"
#include "Ball/TennisBallPhysics.h"
#include "TennisConstants.h"

#include <limits>

namespace
{
	const FVector Gravity(0.0, 0.0, -TennisConst::Gravity);

	// RES-02 の数値実験の条件（参照シミュレーションの Air() の既定：ρ = 1.21、C_d = 0.55）
	FBallSimContext ReferenceContext()
	{
		FBallSimContext Ctx;
		Ctx.Air.Rho = 1.21;
		return Ctx;
	}

	void Fly(FBallState& State, const FBallSimContext& Ctx, int32 Steps)
	{
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			TennisBallPhysics::IntegrateFlight(State, TennisConst::SimDt, Ctx);
		}
	}
}

TEST_CLASS(BallFlight, "Tennis.Sim.Ball")
{
	// T-BALL-005：空気なし・回転なしは、放物線の解析解に 1 mm 以内で一致する
	TEST_METHOD(NoAir_MatchesAnalyticParabola)
	{
		FBallSimContext Ctx;
		Ctx.Air.Rho = 0.0;
		FBallState State;
		State.Pos = FVector(-11.0, 0.5, 1.0);
		State.Vel = FVector(20.0, -1.0, 8.0);
		const FVector P0 = State.Pos;
		const FVector V0 = State.Vel;

		Fly(State, Ctx, 240);
		const double T = 240 * TennisConst::SimDt;  // 1 s

		ASSERT_THAT(IsTrue(FVector::Dist(P0 + V0 * T + 0.5 * Gravity * T * T, State.Pos) < 0.001));
		ASSERT_THAT(IsTrue(FVector::Dist(V0 + Gravity * T, State.Vel) < 1e-9));
		ASSERT_THAT(IsNear(T, State.Time, 1e-9));
	}

	// T-BALL-006：+X に進むボールのトップスピン（+Y 軸まわり）は下向き、バックスピンは上向き
	TEST_METHOD(Topspin_PushesBallDown_BackspinLiftsIt)
	{
		const FBallSimContext Ctx = ReferenceContext();
		const FVector Top = TennisBallPhysics::ComputeAccel(FVector(30.0, 0.0, 0.0), FVector(0.0, 250.0, 0.0), Ctx.Ball, Ctx.Air, 0.0);
		const FVector Back = TennisBallPhysics::ComputeAccel(FVector(30.0, 0.0, 0.0), FVector(0.0, -250.0, 0.0), Ctx.Ball, Ctx.Air, 0.0);
		ASSERT_THAT(IsTrue(Top.Z < -TennisConst::Gravity));
		ASSERT_THAT(IsTrue(Back.Z > -TennisConst::Gravity));
	}

	// T-BALL-007：30 m/s・250 rad/s のマグヌス 5.96 ± 0.05 m/s²（参照 5.9603）。抗力も参照（18.2989）と一致する
	TEST_METHOD(MagnusAndDrag_MatchReference)
	{
		const FBallSimContext Ctx = ReferenceContext();
		const FVector A = TennisBallPhysics::ComputeAccel(FVector(30.0, 0.0, 0.0), FVector(0.0, 250.0, 0.0), Ctx.Ball, Ctx.Air, 0.0);
		ASSERT_THAT(IsNear(-5.96, A.Z + TennisConst::Gravity, 0.05));
		ASSERT_THAT(IsNear(-18.2989, A.X, 0.001));
		ASSERT_THAT(IsNear(0.0, A.Y, 1e-12));
	}

	// T-BALL-008：自由落下の終端速度 21.97 ± 0.1 m/s（k_d = 0.02033 /m。参照 21.9656）
	TEST_METHOD(FreeFall_ReachesTerminalVelocity)
	{
		const FBallSimContext Ctx = ReferenceContext();
		FBallState State;
		State.Pos = FVector(0.0, 0.0, 3000.0);  // 衝突は扱わないので、高さは結果に関係しない
		Fly(State, Ctx, 20 * 240);              // 20 s（時定数の約 9 倍）
		ASSERT_THAT(IsNear(21.97, -State.Vel.Z, 0.1));
	}

	// T-BALL-011：回転は飛んだ距離で減衰する。20 m で 5〜15%（k_ω = 0.005 /m なら約 9.5%）
	TEST_METHOD(Spin_Decays5To15PercentOver20m)
	{
		const FBallSimContext Ctx = ReferenceContext();
		FBallState State;
		State.Pos = FVector(-11.0, 0.0, 1.0);
		State.Vel = FVector(25.0, 0.0, 5.0);
		State.Spin = FVector(0.0, 200.0, 0.0);

		double Path = 0.0;  // 減衰と同じ式（ステップの終わりの速さ × Dt）で数える
		while (Path < 20.0)
		{
			TennisBallPhysics::IntegrateFlight(State, TennisConst::SimDt, Ctx);
			Path += State.Vel.Size() * TennisConst::SimDt;
		}
		const double Ratio = State.Spin.Y / 200.0;
		ASSERT_THAT(IsTrue(1.0 - Ratio >= 0.05 && 1.0 - Ratio <= 0.15));
		ASSERT_THAT(IsNear(FMath::Exp(-Ctx.Ball.SpinDecayPerMeter * Path), Ratio, 1e-9));
		ASSERT_THAT(IsTrue(State.Spin.X == 0.0 && State.Spin.Z == 0.0));  // 軸の向きは変わらない
	}

	// 風：止まっているボールは、相対速度の抗力で風下へ押される
	TEST_METHOD(Wind_PushesBallAtRestDownwind)
	{
		FBallSimContext Ctx = ReferenceContext();
		Ctx.Air.WindMean = FVector(5.0, 0.0, 0.0);
		const FVector A = TennisBallPhysics::ComputeAccel(FVector::ZeroVector, FVector::ZeroVector, Ctx.Ball, Ctx.Air, 0.0);
		ASSERT_THAT(IsTrue(A.X > 0.0));
		ASSERT_THAT(IsNear(-TennisConst::Gravity, A.Z, 1e-12));
	}

	// 境界：空気に対して止まっていれば（回転していても）重力だけ。0 で割らない
	TEST_METHOD(BallAtRest_FeelsOnlyGravity)
	{
		const FBallSimContext Ctx = ReferenceContext();
		const FVector A = TennisBallPhysics::ComputeAccel(FVector::ZeroVector, FVector(0.0, 300.0, 0.0), Ctx.Ball, Ctx.Air, 0.0);
		ASSERT_THAT(IsTrue(A.Equals(Gravity, 0.0)));
	}

	// 境界：回転の軸が進む向きと平行なら、マグヌスは 0（向きが決まらない）
	TEST_METHOD(SpinParallelToVelocity_HasNoMagnus)
	{
		const FBallSimContext Ctx = ReferenceContext();
		const FVector V(30.0, 0.0, 0.0);
		const FVector WithSpin = TennisBallPhysics::ComputeAccel(V, FVector(250.0, 0.0, 0.0), Ctx.Ball, Ctx.Air, 0.0);
		const FVector NoSpin = TennisBallPhysics::ComputeAccel(V, FVector::ZeroVector, Ctx.Ball, Ctx.Air, 0.0);
		ASSERT_THAT(IsTrue(WithSpin.Equals(NoSpin, 1e-12)));
	}

	// 堅牢性：Dt = 0 は何も変えない。負や NaN の Dt は状態を変えない（警告のログ）
	TEST_METHOD(ZeroNegativeOrNaNStep_LeavesStateUnchanged)
	{
		const FBallSimContext Ctx = ReferenceContext();
		FBallState State;
		State.Pos = FVector(1.0, 2.0, 3.0);
		State.Vel = FVector(10.0, 0.0, 2.0);
		State.Spin = FVector(0.0, 100.0, 0.0);
		const FBallState Before = State;

		for (const double Dt : { 0.0, -TennisConst::SimDt, std::numeric_limits<double>::quiet_NaN() })
		{
			TennisBallPhysics::IntegrateFlight(State, Dt, Ctx);
		}
		ASSERT_THAT(IsTrue(State.Pos == Before.Pos && State.Vel == Before.Vel && State.Spin == Before.Spin));
		ASSERT_THAT(IsTrue(State.Time == Before.Time));
	}
};
