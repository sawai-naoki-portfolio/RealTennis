// W-1.5：予測・任意の時刻の状態・時刻まで進める処理。T-BALL-021〜023（DD-01 §3.4・§3.5・§7・§12）。
#include "CQTest.h"
#include "Ball/TennisBallPhysics.h"
#include "Ball/TennisBallPredictor.h"
#include "TennisConstants.h"
#include "TennisTestHelpers.h"

#include <limits>

namespace
{
	constexpr double Dt = TennisConst::SimDt;

	// 共通のラリーの球に、打球の通し番号 7 を付ける（予測が Seq を写すことを確かめるため）
	FBallState RallyShot()
	{
		FBallState State = TennisTest::RallyShot();
		State.Seq = 7;
		return State;
	}

	bool SameState(const FBallState& A, const FBallState& B)
	{
		return A.Pos == B.Pos && A.Vel == B.Vel && A.Spin == B.Spin && A.Time == B.Time && A.NumBounces == B.NumBounces;
	}

	int32 NumBounces(const FTrajectory& Trajectory)
	{
		return TennisTest::CountOf(Trajectory.Events, EBallEventType::Bounce);
	}
}

TEST_CLASS(Predictor, "Tennis.Sim.Ball")
{
	// T-BALL-021：予測と本番の状態が、同じステップ数で完全に一致する
	TEST_METHOD(PredictionMatchesProductionExactly)
	{
		const FBallSimContext Ctx;
		FTrajectory Trajectory;
		FTennisBallPredictor::Predict(RallyShot(), Ctx, FPredictOptions(), Trajectory);

		FBallState Production = RallyShot();
		TArray<FBallEvent> Events;
		ASSERT_THAT(IsTrue(SameState(Production, Trajectory.States[0])));
		for (int32 Index = 1; Index < Trajectory.States.Num(); ++Index)
		{
			TennisBallPhysics::Step(Production, Dt, Ctx, Events);
			ASSERT_THAT(IsTrue(SameState(Production, Trajectory.States[Index])));
		}
		ASSERT_THAT(AreEqual(Events.Num(), Trajectory.Events.Num()));
		ASSERT_THAT(AreEqual(7, Trajectory.Seq));
	}

	// DD-01 §7：バウンド 2 回で打ち切る。最初のバウンドは相手のコートで、時刻の順
	TEST_METHOD(Prediction_StopsAtSecondBounce)
	{
		FTrajectory Trajectory;
		FTennisBallPredictor::Predict(RallyShot(), FBallSimContext(), FPredictOptions(), Trajectory);
		ASSERT_THAT(AreEqual(2, NumBounces(Trajectory)));
		const FBallEvent* First = Trajectory.FirstBounce();
		ASSERT_THAT(IsTrue(First != nullptr && First->Pos.X > 0.0));
		ASSERT_THAT(IsTrue(Trajectory.States.Last().Time < 4.0));
		ASSERT_THAT(IsTrue(Trajectory.States.Last().Time - Trajectory.Events.Last().Time < Dt));  // 2 回目のステップで止まる
	}

	// DD-01 §7：時間で打ち切る（States は打ち出し＋ステップの数）。前の中身は消す
	TEST_METHOD(Prediction_StopsAtMaxTime)
	{
		FTrajectory Trajectory;
		Trajectory.Events.AddDefaulted(3);
		FPredictOptions Opt;
		Opt.MaxTime = 0.5;
		FTennisBallPredictor::Predict(RallyShot(), FBallSimContext(), Opt, Trajectory);
		ASSERT_THAT(AreEqual(121, Trajectory.States.Num()));
		ASSERT_THAT(IsNear(0.5, Trajectory.States.Last().Time, 1e-12));
		ASSERT_THAT(IsTrue(Trajectory.Events.ContainsByPredicate([](const FBallEvent& E) { return E.Type == EBallEventType::NetCross; })));
		ASSERT_THAT(AreEqual(0, NumBounces(Trajectory)));
	}

	// 堅牢性：不正な打ち切りの値は既定値にする（警告のログ）
	TEST_METHOD(InvalidOptions_FallBackToDefaults)
	{
		FPredictOptions Opt;
		Opt.MaxTime = std::numeric_limits<double>::quiet_NaN();
		Opt.MaxBounces = 0;
		FTrajectory Trajectory;
		FTennisBallPredictor::Predict(RallyShot(), FBallSimContext(), Opt, Trajectory);
		ASSERT_THAT(AreEqual(2, NumBounces(Trajectory)));
	}

	// T-BALL-023：GetStateAt(T) は、T まで本番で進めた状態と一致する（ステップの境目では完全に、端数では同じ Step で）
	TEST_METHOD(GetStateAt_MatchesProduction)
	{
		const FBallSimContext Ctx;
		FTrajectory Trajectory;
		FTennisBallPredictor::Predict(RallyShot(), Ctx, FPredictOptions(), Trajectory);

		FBallState Production = RallyShot();
		TArray<FBallEvent> Events;
		for (int32 Index = 0; Index < 168; ++Index)
		{
			TennisBallPhysics::Step(Production, Dt, Ctx, Events);
		}
		FBallState AtStep;
		ASSERT_THAT(IsTrue(FTennisBallPredictor::GetStateAt(Trajectory, Production.Time, Ctx, AtStep)));
		ASSERT_THAT(IsTrue(SameState(Production, AtStep)));

		const double Fraction = 0.37 * Dt;
		TennisBallPhysics::Step(Production, Fraction, Ctx, Events);
		FBallState Between;
		ASSERT_THAT(IsTrue(FTennisBallPredictor::GetStateAt(Trajectory, Production.Time, Ctx, Between)));
		ASSERT_THAT(IsTrue(Between.Pos.Equals(Production.Pos, 1e-12) && Between.Vel.Equals(Production.Vel, 1e-12)));
	}

	// 境界：予測の範囲の外と NaN は false で、Out を変えない
	TEST_METHOD(GetStateAt_OutsideTheTrajectory_ReturnsFalse)
	{
		const FBallSimContext Ctx;
		FTrajectory Trajectory;
		FBallState Out;
		Out.Seq = 99;
		ASSERT_THAT(IsFalse(FTennisBallPredictor::GetStateAt(Trajectory, 0.0, Ctx, Out)));  // 空
		FTennisBallPredictor::Predict(RallyShot(), Ctx, FPredictOptions(), Trajectory);
		for (const double Time : { -0.001, Trajectory.States.Last().Time + 0.001, std::numeric_limits<double>::quiet_NaN() })
		{
			ASSERT_THAT(IsFalse(FTennisBallPredictor::GetStateAt(Trajectory, Time, Ctx, Out)));
		}
		ASSERT_THAT(AreEqual(99, Out.Seq));
	}

	// T-BALL-022 の中身：過去の時刻（ステップの境目でない）の打ち出しを今の時刻まで進めると、時刻はちょうど今になり、
	// 同じ打ち出しを同じように進めたものと完全に一致する
	TEST_METHOD(AdvanceTo_ReachesTargetTimeDeterministically)
	{
		const FBallSimContext Ctx;
		FBallState Launch = RallyShot();
		Launch.Time = 10.0123;
		const double Now = 10.5;

		FBallState A = Launch;
		FBallState B = Launch;
		TArray<FBallEvent> EventsA;
		TArray<FBallEvent> EventsB;
		ASSERT_THAT(IsTrue(TennisBallPhysics::AdvanceTo(A, Now, Ctx, EventsA)));
		ASSERT_THAT(IsTrue(TennisBallPhysics::AdvanceTo(B, Now, Ctx, EventsB)));
		ASSERT_THAT(IsTrue(A.Time == Now));
		ASSERT_THAT(IsTrue(SameState(A, B) && EventsA.Num() == EventsB.Num()));

		// 打ち出しから同じ時間だけ予測したものとも、許容の範囲で一致する（端数のステップの位置だけが違う）
		FTrajectory Trajectory;
		FTennisBallPredictor::Predict(Launch, Ctx, FPredictOptions(), Trajectory);
		FBallState Predicted;
		ASSERT_THAT(IsTrue(FTennisBallPredictor::GetStateAt(Trajectory, Now, Ctx, Predicted)));
		ASSERT_THAT(IsTrue(Predicted.Pos.Equals(A.Pos, 1e-9)));
	}

	// 堅牢性：過去へ戻す・上限より先・NaN は、状態を変えずに false（警告のログ）
	TEST_METHOD(AdvanceTo_RejectsInvalidTargets)
	{
		const FBallSimContext Ctx;
		FBallState State = RallyShot();
		State.Time = 5.0;
		const FBallState Before = State;
		TArray<FBallEvent> Events;
		for (const double Target : { 4.9, 5.0 + TennisBallPhysics::MaxAdvanceTime + 1.0, std::numeric_limits<double>::quiet_NaN() })
		{
			ASSERT_THAT(IsFalse(TennisBallPhysics::AdvanceTo(State, Target, Ctx, Events)));
		}
		ASSERT_THAT(IsTrue(SameState(Before, State) && Events.IsEmpty()));
	}
};
