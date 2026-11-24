// W-1.3（前半）：地面のバウンドの撃力。T-BALL-004（仮想 CPR 試験）・T-BALL-010（グリップと滑り）（RES-02 §6）。
// 期待値は参照シミュレーション Tools/Reference/ballsim_reference.py（cpr_test・BENCH）の値。
#include "CQTest.h"
#include "Ball/TennisBallPhysics.h"
#include "Calibration/CourtCalibration.h"

namespace
{
	struct FSurfaceCase
	{
		const TCHAR* Name;
		double E, Mu, Et;       // DATA-02 §2
		double ExpectedCpr;     // 参照の cpr_test
		int32 ExpectedCategory; // ITF の区分
	};

	FSurfaceParams MakeSurface(double E, double Mu, double Et)
	{
		FSurfaceParams Surface;
		Surface.RestitutionRef = E;
		Surface.Friction = Mu;
		Surface.TangentialRestitution = Et;
		return Surface;
	}

	// 参照のモデルと同じく、e を速さ・気温によらず一定にした面（BENCH の HARD_E・HARD_MU）
	FSurfaceParams ReferenceHard()
	{
		FSurfaceParams Surface = MakeSurface(0.80, 0.63, 0.0);
		Surface.RestitutionSpeedSlope = 0.0;
		Surface.TempCoeff = 0.0;
		return Surface;
	}

	double ContactSlip(const FBallState& State, const FBallParams& Ball)
	{
		return (State.Vel + FVector::CrossProduct(State.Spin, FVector(0.0, 0.0, -Ball.Radius))).X;
	}
}

TEST_CLASS(GroundBounce, "Tennis.Sim.Ball")
{
	// T-BALL-004：5 面が目標の区分に入り、CPR が参照の値と一致する（合格の条件は目標 ±2、ここでは参照と ±0.05）
	TEST_METHOD(VirtualCprTest_MatchesTargetCategories)
	{
		const FSurfaceCase Cases[] = {
			{ TEXT("Clay"),        0.84, 0.90, 0.03, 26.2286, 1 },
			{ TEXT("HardMedium"),  0.80, 0.63, 0.00, 38.50,   3 },
			{ TEXT("HardMedFast"), 0.79, 0.60, 0.00, 43.00,   4 },
			{ TEXT("Grass"),       0.72, 0.62, 0.00, 51.50,   5 },
			{ TEXT("IndoorFast"),  0.77, 0.55, 0.00, 51.00,   5 },
		};
		for (const FSurfaceCase& Case : Cases)
		{
			const FCprResult Result = TennisCourtCalibration::VirtualCprTest(MakeSurface(Case.E, Case.Mu, Case.Et), FBallParams());
			ASSERT_THAT(IsNear(Case.ExpectedCpr, Result.Cpr, 0.05, Case.Name));
			ASSERT_THAT(AreEqual(Case.ExpectedCategory, Result.Category, Case.Name));
		}
	}

	// T-BALL-010：トップスピンのラリーはグリップ（接触点の滑りが 0 になる）、フラットとスライスは滑る。
	// 入射の速度と回転は参照の BENCH の最初のバウンド、出射の速度は参照の値
	TEST_METHOD(TopspinGrips_FlatAndSliceSlide)
	{
		struct FBenchCase { const TCHAR* Name; FVector VelIn; double SpinY; FVector VelOut; bool bSlides; };
		const FBenchCase Cases[] = {
			{ TEXT("rally_fh"), FVector(22.2561, 0.0, -6.1644),  240.8554, FVector(17.2219, 0.0, 4.9315), false },
			{ TEXT("heavy_fh"), FVector(21.2634, 0.0, -6.5366),  335.1032, FVector(17.7017, 0.0, 5.2293), false },
			{ TEXT("flat_fh"),  FVector(27.3704, 0.0, -4.8979),   83.7758, FVector(21.8161, 0.0, 3.9184), true },
			{ TEXT("bh_slice"), FVector(18.2005, 0.0, -3.8356), -157.0796, FVector(13.8510, 0.0, 3.0685), true },
		};
		const FBallParams Ball;
		for (const FBenchCase& Case : Cases)
		{
			FBallState State;
			State.Vel = Case.VelIn;
			State.Spin = FVector(0.0, Case.SpinY, 0.0);
			TennisBallPhysics::ResolveGroundBounce(State, Ball, ReferenceHard(), FSurfaceParams::ReferenceTempC);
			ASSERT_THAT(IsTrue(State.Vel.Equals(Case.VelOut, 1e-3), Case.Name));
			ASSERT_THAT(AreEqual(Case.bSlides, FMath::Abs(ContactSlip(State, Ball)) > 1e-6, Case.Name));
		}
	}

	// DD-01 §4.1：e_court = e_surface × (e_ball / e_ball(Type 2))。Type 1 は高く、高地球は低く弾む
	TEST_METHOD(BallType_ScalesRestitution)
	{
		FSurfaceParams Surface = ReferenceHard();
		const auto ReboundSpeed = [&Surface](double BallRestitution)
		{
			FBallParams Ball;
			Ball.RestitutionHard = BallRestitution;
			FBallState State;
			State.Vel = FVector(0.0, 0.0, -7.0);
			TennisBallPhysics::ResolveGroundBounce(State, Ball, Surface, FSurfaceParams::ReferenceTempC);
			return State.Vel.Z;
		};
		ASSERT_THAT(IsNear(0.80 * 7.0, ReboundSpeed(FBallParams::Type2RestitutionHard), 1e-9));
		ASSERT_THAT(IsNear(0.80 * 0.785 / 0.775 * 7.0, ReboundSpeed(0.785), 1e-9));
		ASSERT_THAT(IsNear(0.80 * 0.740 / 0.775 * 7.0, ReboundSpeed(0.740), 1e-9));
	}

	// 境界：離れていく球には何もしない。数えるのは実際のバウンドだけ
	TEST_METHOD(BallMovingUp_IsUnchanged)
	{
		FBallState State;
		State.Vel = FVector(10.0, 0.0, 0.5);
		State.Spin = FVector(0.0, 100.0, 0.0);
		const FBallState Before = State;
		TennisBallPhysics::ResolveGroundBounce(State, FBallParams(), FSurfaceParams(), 20.0);
		ASSERT_THAT(IsTrue(State.Vel == Before.Vel && State.Spin == Before.Spin));
		ASSERT_THAT(AreEqual(0, State.NumBounces));
	}

	// 境界：摩擦 0 なら接線方向は変わらない。バウンドを 1 回数える
	TEST_METHOD(Frictionless_KeepsTangentialVelocityAndSpin)
	{
		FBallState State;
		State.Vel = FVector(20.0, 3.0, -6.0);
		State.Spin = FVector(0.0, 150.0, 0.0);
		TennisBallPhysics::ResolveGroundBounce(State, FBallParams(), MakeSurface(0.80, 0.0, 0.0), 23.0);
		ASSERT_THAT(IsNear(20.0, State.Vel.X, 1e-12));
		ASSERT_THAT(IsNear(3.0, State.Vel.Y, 1e-12));
		ASSERT_THAT(IsTrue(State.Spin == FVector(0.0, 150.0, 0.0)));
		ASSERT_THAT(AreEqual(1, State.NumBounces));
	}
};
