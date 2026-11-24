// W-3.5、T-STRK-013：ストロークが決まった後の打点の選び直し（DD-03 §6.2）。ボールの高さがスイートスポットの高さにいちばん近い時刻を選ぶ。
#include "CQTest.h"
#include "Ball/TennisBallPredictor.h"
#include "Reach/TennisReach.h"
#include "Shot/TennisShotSolver.h"
#include "Stroke/TennisStrokeTiming.h"
#include "TennisSimLog.h"

#include <limits>

namespace
{
	const FVector ForehandOffset(0.38, 0.82, 1.02);  // 仮のフォアの焼いた値（前・右・高さ。m）
	const FVector2D Aim(9.0, 0.0);                    // ニュートラルの狙い（DD-03 §6.4）

	/** 1 回バウンドした後の、(時刻, 高さ) の列の軌道。位置は (−10, 1.2)。バウンドは 0.9 s */
	FTrajectory Samples(std::initializer_list<TPair<double, double>> TimeAndHeight, int32 NumBounces = 1)
	{
		FTrajectory Trajectory;
		FBallEvent Bounce;
		Bounce.Type = EBallEventType::Bounce;
		Bounce.Time = 0.9;
		Trajectory.Events.Add(Bounce);
		for (const TPair<double, double>& Sample : TimeAndHeight)
		{
			FBallState State;
			State.Time = Sample.Key;
			State.Pos = FVector(-10.0, 1.2, Sample.Value);
			State.NumBounces = Sample.Key < Bounce.Time ? 0 : NumBounces;
			Trajectory.States.Add(State);
		}
		return Trajectory;
	}

	FHitCandidate CandidateAt(const FTrajectory& Trajectory, int32 Index)
	{
		FHitCandidate Candidate;
		Candidate.Time = Trajectory.States[Index].Time;
		Candidate.BallPos = Trajectory.States[Index].Pos;
		Candidate.Kind = EHitKind::Groundstroke;
		Candidate.Side = EStrokeSide::Forehand;
		return Candidate;
	}

	FReachQuery Player(const FVector2D& Position)
	{
		FReachQuery Q;
		Q.Position = Position;
		Q.PrepTime = FReachParams().DefaultPrepTime;
		return Q;
	}

	FHitCandidate Refine(const FTrajectory& Trajectory, const FHitCandidate& Candidate, const FReachQuery& Q, const FVector& Offset = ForehandOffset)
	{
		return TennisReach::RefineForStroke(Trajectory, Candidate, Offset, Aim, Q, FMovementParams(), FReachParams(), FStrokeSyncParams());
	}

	/** ボールの位置 Ball で、右利きのフォアのスイートスポットをボールに重ねる体の目標（狙いは AimPoint） */
	FVector2D BodyFor(const FVector2D& Ball, double SideSign = -1.0, const FVector2D& AimPoint = Aim)
	{
		return TennisStrokeTiming::BodyTarget(Ball, TennisStrokeTiming::FacingYaw(Ball, AimPoint, SideSign, FStrokeSyncParams()), FVector2D(0.38, 0.82));
	}

	/** 体の位置が、選手の側の移動の範囲（DD-03 §2.4）の中か */
	bool IsInsideArea(const FVector2D& Body, double SideSign = -1.0)
	{
		FVector2D Clamped = Body;
		FVector2D Velocity = FVector2D::ZeroVector;
		TennisMovement::ClampToArea(Clamped, Velocity, SideSign);
		return Clamped.Equals(Body, 0.01);
	}
}

TEST_CLASS(TennisReachRefine, "Tennis.Sim.Reach.Refine")
{
	const FVector2D Near{ -10.4, 0.4 };  // 体の目標のすぐ近く（どのサンプルにも間に合う）

	// 範囲（前後 0.25 s）の中で、高さがいちばん近いサンプルを選ぶ。範囲の外・バウンドの前のサンプルは使わない
	TEST_METHOD(Refine_PicksTheSampleNearestTheSweetSpotHeight)
	{
		// 0.88 s はバウンドの前。1.60 s は範囲の外。1.20 s と 1.30 s は同じ近さ（差 0.02 m）
		const FTrajectory Trajectory = Samples({ { 0.88, 1.02 }, { 1.00, 0.80 }, { 1.10, 0.95 }, { 1.20, 1.04 }, { 1.30, 1.00 }, { 1.60, 1.02 } });
		const FHitCandidate Refined = Refine(Trajectory, CandidateAt(Trajectory, 2), Player(Near));
		ASSERT_THAT(IsNear(1.20, Refined.Time, 1e-12));
		ASSERT_THAT(IsNear(1.04, Refined.BallPos.Z, 1e-12));
		ASSERT_THAT(IsTrue(Refined.Kind == EHitKind::Groundstroke && !Refined.bStretch));

		// 体の目標は、狙いへの向きで、スイートスポットの水平のずれがボールに重なる位置（TennisStrokeTiming と同じ式）
		const FVector2D Ball(-10.0, 1.2);
		const FVector2D Expected = TennisStrokeTiming::BodyTarget(Ball, TennisStrokeTiming::FacingYaw(Ball, Aim, -1.0, FStrokeSyncParams()), FVector2D(0.38, 0.82));
		ASSERT_THAT(IsTrue(Refined.BodyTarget.Equals(Expected, 1e-12)));
		ASSERT_THAT(IsNear(TennisReach::TimeToReach(Player(Near), Expected, FMovementParams()), Refined.TimeToReach, 1e-12));
	}

	// 同じ近さなら、候補の時刻に近いほう（後に調べるサンプルでも）
	TEST_METHOD(Refine_BreaksTiesTowardTheCandidateTime)
	{
		// 0.95 s と 1.05 s は、スイートスポットの高さ（1.02 m）との差がどちらも 0.02 m。候補は 1.10 s
		const FTrajectory Trajectory = Samples({ { 0.95, 1.00 }, { 1.05, 1.04 }, { 1.10, 0.80 } });
		ASSERT_THAT(IsNear(1.05, Refine(Trajectory, CandidateAt(Trajectory, 2), Player(Near)).Time, 0.0));
	}

	// 候補と同じバウンドの間でも、種類（ハーフボレー）や高さの帯が違うサンプルは、高さが近くても使わない
	TEST_METHOD(Refine_IgnoresOtherKindsAndSamplesOutsideTheBand)
	{
		// スイートスポットの高さ 0.30 m。0.95 s はバウンド（0.9 s）の直後の低いボール（ハーフボレー、差 0）。1.20 s はグラウンドストロークの帯
		// （0.25 m から）の下（差 0.08 m）。1.10 s が候補（差 0.20 m）、1.30 s は帯の中（差 0.10 m）
		const FTrajectory Trajectory = Samples({ { 0.95, 0.30 }, { 1.10, 0.50 }, { 1.20, 0.22 }, { 1.30, 0.40 } });
		const FHitCandidate Refined = Refine(Trajectory, CandidateAt(Trajectory, 1), Player(Near), FVector(0.38, 0.82, 0.30));
		ASSERT_THAT(IsNear(1.30, Refined.Time, 0.0));
	}

	// 体の目標が移動の範囲（サイドラインの外 3.5 m まで）の外に出るサンプルは、高さが合って間に合っても使わない
	TEST_METHOD(Refine_IgnoresSamplesWhoseBodyTargetIsOutsideTheArea)
	{
		// ボールはサイドラインの外。1.30 s が候補（差 0.22 m）、1.40 s は高さが合う（差 0）が体の目標が範囲の外、1.50 s は範囲の中（差 0.07 m）
		FTrajectory Trajectory = Samples({ { 1.30, 0.80 }, { 1.40, 1.02 }, { 1.50, 0.95 } });
		const FVector2D Inner(-10.0, -7.5), Outer(-10.0, -9.0);
		Trajectory.States[0].Pos = FVector(Inner, 0.80);
		Trajectory.States[1].Pos = FVector(Outer, 1.02);
		Trajectory.States[2].Pos = FVector(Inner, 0.95);
		const FReachQuery Q = Player(FVector2D(-10.4, -8.9));  // 範囲の端の近く
		ASSERT_THAT(IsTrue(IsInsideArea(BodyFor(Inner)) && !IsInsideArea(BodyFor(Outer))));  // 前提
		ASSERT_THAT(IsTrue(TennisReach::TimeToReach(Q, BodyFor(Outer), FMovementParams()) <= 1.40 - Q.PrepTime));  // 前提：間に合う

		const FHitCandidate Refined = Refine(Trajectory, CandidateAt(Trajectory, 0), Q);
		ASSERT_THAT(IsNear(1.50, Refined.Time, 0.0));
		ASSERT_THAT(IsTrue(IsInsideArea(Refined.BodyTarget)));
	}

	// 相手の側（ネットの向こう）のサンプルは、体の目標が範囲の中に入り、高さが合って間に合っても使わない
	TEST_METHOD(Refine_IgnoresSamplesOnTheOpponentsSide)
	{
		// ネット際のボレー（どれもバウンドの前）。0.75 s はネットの向こう（X > 0）で高さが合う。0.85 s が候補（差 0.22 m）
		FTrajectory Trajectory = Samples({ { 0.75, 1.02 }, { 0.85, 0.80 } });
		const FVector2D Over(0.1, 6.3), Here(-0.3, 6.3);
		Trajectory.States[0].Pos = FVector(Over, 1.02);
		Trajectory.States[1].Pos = FVector(Here, 0.80);
		FHitCandidate Candidate = CandidateAt(Trajectory, 1);
		Candidate.Kind = EHitKind::Volley;
		const FReachQuery Q = Player(BodyFor(Over));  // 向こうのサンプルの体の目標に立っている
		ASSERT_THAT(IsTrue(IsInsideArea(BodyFor(Over))));  // 前提：範囲の中（範囲の判定では除かれない）

		const FHitCandidate Refined = Refine(Trajectory, Candidate, Q);
		ASSERT_THAT(IsNear(0.85, Refined.Time, 0.0));
		ASSERT_THAT(IsTrue(Refined.BodyTarget.Equals(BodyFor(Here), 1e-12)));
	}

	// 奥の選手（SideSign +1。X > 0 の側で −X を向く）も同じ規則と同じ式
	TEST_METHOD(Refine_WorksForThePlayerOnTheFarSide)
	{
		FTrajectory Trajectory = Samples({ { 1.00, 0.80 }, { 1.10, 0.95 }, { 1.20, 1.03 } });
		const FVector2D Ball(10.0, -1.2);
		for (FBallState& State : Trajectory.States)
		{
			State.Pos = FVector(Ball, State.Pos.Z);
		}
		FReachQuery Q = Player(FVector2D(10.4, -0.4));
		Q.SideSign = 1.0;
		const FVector2D FarAim(-9.0, 0.0);
		const FHitCandidate Refined = TennisReach::RefineForStroke(Trajectory, CandidateAt(Trajectory, 1), ForehandOffset, FarAim, Q, FMovementParams(),
			FReachParams(), FStrokeSyncParams());
		ASSERT_THAT(IsNear(1.20, Refined.Time, 0.0));
		const FVector2D Expected = BodyFor(Ball, 1.0, FarAim);
		ASSERT_THAT(IsTrue(Refined.BodyTarget.Equals(Expected, 1e-12)));
		ASSERT_THAT(IsTrue(Expected.X > 10.3 && Expected.Y > -0.5));  // ボールの後ろ（ネットから遠い側）で、右（−Y）にボール
	}

	// 候補がいちばん近ければ、時刻は変えない（体の目標だけを置き直す）
	TEST_METHOD(Refine_KeepsTheCandidateWhenItIsAlreadyTheNearest)
	{
		const FTrajectory Trajectory = Samples({ { 1.00, 0.90 }, { 1.10, 1.02 }, { 1.20, 0.95 } });
		const FHitCandidate Refined = Refine(Trajectory, CandidateAt(Trajectory, 1), Player(Near));
		ASSERT_THAT(IsNear(1.10, Refined.Time, 0.0));
		ASSERT_THAT(IsTrue(Refined.BodyTarget.X < -10.3 && Refined.BodyTarget.Y < 0.5));
	}

	// 間に合うサンプルがなければ（遠い）、候補の時刻のまま。体の目標は置き直し、ストレッチの印はそのまま
	TEST_METHOD(Refine_KeepsTheCandidateTimeWhenNothingElseIsReachable)
	{
		const FTrajectory Trajectory = Samples({ { 1.00, 0.80 }, { 1.10, 0.95 }, { 1.20, 1.02 } });
		FHitCandidate Candidate = CandidateAt(Trajectory, 1);
		Candidate.bStretch = true;
		const FHitCandidate Refined = Refine(Trajectory, Candidate, Player(FVector2D(-10.4, -6.0)));  // 6 m 以上離れている
		ASSERT_THAT(IsNear(1.10, Refined.Time, 0.0));
		ASSERT_THAT(IsTrue(Refined.bStretch));
		ASSERT_THAT(IsTrue(Refined.BodyTarget.Y > 0.3 && Refined.BodyTarget.Y < 0.5));
	}

	// 左利きは、スイートスポットの左右を反転する。今より前のサンプルは使わない。数でない入力は候補のまま
	TEST_METHOD(Refine_MirrorsForLeftHandersAndIgnoresThePastAndInvalidInput)
	{
		const FTrajectory Trajectory = Samples({ { 1.00, 1.02 }, { 1.10, 0.95 }, { 1.20, 0.90 } });
		FReachQuery Left = Player(FVector2D(-10.4, 2.0));
		Left.Handedness = ETennisHandedness::Left;
		ASSERT_THAT(IsTrue(Refine(Trajectory, CandidateAt(Trajectory, 1), Left).BodyTarget.Y > 1.9));  // 体はボールの右（+Y）の側

		FReachQuery Late = Player(Near);
		Late.Now = 1.05;      // 1.00 s のサンプル（高さが合う）は過去
		Late.PrepTime = 0.0;
		ASSERT_THAT(IsNear(1.10, Refine(Trajectory, CandidateAt(Trajectory, 1), Late).Time, 0.0));

		const double NaN = std::numeric_limits<double>::quiet_NaN();
		const FHitCandidate Candidate = CandidateAt(Trajectory, 1);
		const FHitCandidate Invalid = Refine(Trajectory, Candidate, Player(Near), FVector(0.38, 0.82, NaN));
		ASSERT_THAT(IsTrue(Invalid.Time == Candidate.Time && Invalid.BodyTarget == Candidate.BodyTarget));

		FHitCandidate NoTime = Candidate;  // 時刻が数でなければ、範囲が決まらない
		NoTime.Time = NaN;
		const FHitCandidate KeptTime = Refine(Trajectory, NoTime, Player(Near));
		ASSERT_THAT(IsTrue(FMath::IsNaN(KeptTime.Time) && KeptTime.BodyTarget == NoTime.BodyTarget));

		FHitCandidate NoHeight = Candidate;  // 高さが数でなければ、候補と比べられない
		NoHeight.BallPos.Z = NaN;
		const FHitCandidate KeptHeight = Refine(Trajectory, NoHeight, Player(Near));
		ASSERT_THAT(IsTrue(KeptHeight.Time == NoHeight.Time && KeptHeight.BodyTarget == NoHeight.BodyTarget));
	}

	// 候補と同じバウンドの間だけを見る：バウンドの後のスマッシュの候補は、バウンドの前のサンプルを、高さが合っていても選ばない
	TEST_METHOD(Refine_StaysWithinTheCandidatesBounceInterval)
	{
		// どれもスマッシュの帯（2.2〜3.2 m）。0.80 s はバウンド（0.9 s）の前で、高さが合い、間に合う。0.95 s（候補）と 1.05 s はバウンドの後
		const FTrajectory Trajectory = Samples({ { 0.80, 2.60 }, { 0.95, 2.30 }, { 1.05, 2.45 } });
		FHitCandidate Candidate = CandidateAt(Trajectory, 1);
		Candidate.Kind = EHitKind::Smash;
		const FHitCandidate Refined = Refine(Trajectory, Candidate, Player(Near), FVector(0.38, 0.82, 2.60));
		ASSERT_THAT(IsNear(1.05, Refined.Time, 0.0));
		ASSERT_THAT(IsTrue(Refined.Kind == EHitKind::Smash));
	}

	// 実際の軌道（シードを決めた球）：探索が選んだ中くらいの高さのグラウンドストロークの候補を選び直すと、スイートスポットの高さとの差が
	// 大きく減る（T-STRK-013 の実測：15.0 → 3.6 cm）。選び直した打点は、間に合い、体の目標が移動の範囲の中で、種類が変わらない
	TEST_METHOD(Refine_ShrinksTheHeightGapOnRealTrajectories)
	{
		const FBallSimContext Ctx;
		const FTennisShotSolver Solver(Ctx);
		const FReachParams Reach;
		const FStrokeSyncParams Sync;
		FRandomStream Random(20261113);
		double Before = 0.0, After = 0.0;
		int32 Count = 0;
		for (int32 Shot = 0; Shot < 4000 && Count < 300; ++Shot)
		{
			FShotRequest Request;
			Request.Origin = FVector(11.0, Random.FRandRange(-2.0, 2.0), 1.0);
			Request.Target = FVector(Random.FRandRange(-11.0, -5.5), Random.FRandRange(-3.5, 3.5), 0.0);
			Request.ConstraintValue = Random.FRandRange(0.5, 2.0);
			Request.MinSpeed = 14.0;
			Request.MaxSpeed = 40.0;
			Request.Rpm = Random.FRandRange(-1500.0, 3200.0);
			const FShotSolveResult Solved = Solver.Solve(Request);
			FReachQuery Q = Player(FVector2D(-12.5, 0.0));
			Q.Now = Random.FRandRange(0.0, 0.4);
			if (!Solved.bSolved)
			{
				continue;
			}
			FTrajectory Trajectory;
			FTennisBallPredictor::Predict(Solved.Launch, Ctx, FPredictOptions(), Trajectory);
			TArray<FHitCandidate> Candidates;
			TennisReach::FindCandidates(Trajectory, Q, FMovementParams(), Reach, Candidates);
			if (Candidates.IsEmpty() || Candidates[0].Kind != EHitKind::Groundstroke || Candidates[0].bStretch
				|| Candidates[0].BallPos.Z < 0.7 || Candidates[0].BallPos.Z >= 1.3)
			{
				continue;  // M3 の対象（DD-03 §12.1）だけ
			}
			const FHitCandidate& Candidate = Candidates[0];
			const FVector Offset = Candidate.Side == EStrokeSide::Forehand ? ForehandOffset : FVector(0.38, -0.62, 1.083);
			const FHitCandidate Refined = TennisReach::RefineForStroke(Trajectory, Candidate, Offset, Aim, Q, FMovementParams(), Reach, Sync);
			const double GapBefore = FMath::Abs(Candidate.BallPos.Z - Offset.Z);
			const double GapAfter = FMath::Abs(Refined.BallPos.Z - Offset.Z);
			ASSERT_THAT(IsTrue(GapAfter <= GapBefore + 1e-9));
			ASSERT_THAT(IsTrue(FMath::Abs(Refined.Time - Candidate.Time) <= Sync.HitRefineWindow + 1e-9 && Refined.Time >= Q.Now));
			ASSERT_THAT(IsTrue(Refined.Kind == Candidate.Kind && Refined.BallPos.X < 0.0));
			if (Refined.Time != Candidate.Time)
			{
				ASSERT_THAT(IsTrue(Refined.TimeToReach <= Refined.Time - Q.Now - Q.PrepTime + 1e-9 && IsInsideArea(Refined.BodyTarget)));
			}
			Before += GapBefore;
			After += GapAfter;
			++Count;
		}
		ASSERT_THAT(AreEqual(300, Count));
		UE_LOG(LogTennisStroke, Display, TEXT("[RefineTest] balls=%d height_gap_before=%.4f height_gap_after=%.4f"), Count, Before / Count, After / Count);
		ASSERT_THAT(IsTrue(Before / Count > 0.10, *FString::Printf(TEXT("before %.4f m"), Before / Count)));  // 選び直す前は 10 cm より大きい
		ASSERT_THAT(IsTrue(After / Count < 0.05, *FString::Printf(TEXT("after %.4f m"), After / Count)));    // 選び直すと 5 cm 未満
	}
};
