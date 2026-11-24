// W-3.3：打点の候補の探索（DD-03 §4.4・§4.5、ACT-03、T-STRK-001・008）。
#include "CQTest.h"
#include "Ball/TennisBallPredictor.h"
#include "Reach/TennisReach.h"
#include "Shot/TennisShotSolver.h"

namespace
{
	/** サンプル 1 つの軌道（BounceTime ≥ 0 なら、その時刻にバウンドのイベントを置く） */
	FTrajectory OneSample(const FVector& Pos, double Time, int32 NumBounces, double BounceTime = -1.0)
	{
		FTrajectory Trajectory;
		FBallState State;
		State.Pos = Pos;
		State.Time = Time;
		State.NumBounces = NumBounces;
		Trajectory.States.Add(State);
		if (BounceTime >= 0.0)
		{
			FBallEvent Bounce;
			Bounce.Type = EBallEventType::Bounce;
			Bounce.Time = BounceTime;
			Trajectory.Events.Add(Bounce);
		}
		return Trajectory;
	}

	FReachQuery Player(const FVector2D& Position, double SideSign = -1.0, ETennisHandedness Hand = ETennisHandedness::Right)
	{
		FReachQuery Q;
		Q.Position = Position;
		Q.SideSign = SideSign;
		Q.Handedness = Hand;
		Q.PrepTime = FReachParams().DefaultPrepTime;  // 0.30 s
		return Q;
	}

	TArray<FHitCandidate> Find(const FTrajectory& Trajectory, const FReachQuery& Q)
	{
		TArray<FHitCandidate> Out;
		TennisReach::FindCandidates(Trajectory, Q, FMovementParams(), FReachParams(), Out);
		return Out;
	}
}

TEST_CLASS(TennisReachCandidates, "Tennis.Sim.Reach.FindCandidates")
{
	const FVector2D Home{ -11.0, 0.0 };

	// T-STRK-008：種類は高さの帯とバウンドで決まる。帯の外・相手の側・2 回目のバウンドの後・Now より前は候補にしない
	TEST_METHOD(Kinds_FollowTheBandsAndBounces)
	{
		struct FCase { FVector Pos; int32 Bounces; double BounceTime; bool bExpected; EHitKind Kind; const TCHAR* Name; };
		const FCase Cases[] = {
			{ FVector(-10.0, 1.2, 1.0), 0, -1.0, true, EHitKind::Volley, TEXT("volley") },
			{ FVector(-10.0, 1.2, 1.0), 1, 1.0, true, EHitKind::Groundstroke, TEXT("groundstroke") },
			{ FVector(-10.0, 1.2, 0.2), 1, 1.95, true, EHitKind::HalfVolley, TEXT("half volley") },
			{ FVector(-10.0, 1.2, 2.6), 1, 1.0, true, EHitKind::Smash, TEXT("smash") },
			{ FVector(-10.0, 1.2, 0.1), 1, 1.0, false, EHitKind::Groundstroke, TEXT("too low after the bounce") },
			{ FVector(-10.0, 1.2, 1.9), 1, 1.0, false, EHitKind::Groundstroke, TEXT("between the bands") },
			{ FVector(-10.0, 1.2, 1.0), 2, 1.0, false, EHitKind::Groundstroke, TEXT("after the second bounce") },
			{ FVector(5.0, 1.2, 1.0), 1, 1.0, false, EHitKind::Groundstroke, TEXT("opponent's side") },
		};
		for (const FCase& Case : Cases)
		{
			const TArray<FHitCandidate> Out = Find(OneSample(Case.Pos, 2.0, Case.Bounces, Case.BounceTime), Player(Home));
			ASSERT_THAT(AreEqual(Case.bExpected ? 1 : 0, Out.Num(), Case.Name));
			if (Case.bExpected)
			{
				ASSERT_THAT(IsTrue(Out[0].Kind == Case.Kind && !Out[0].bStretch, Case.Name));
			}
		}

		FReachQuery Late = Player(Home);
		Late.Now = 2.5;
		ASSERT_THAT(AreEqual(0, Find(OneSample(FVector(-10.0, 1.2, 1.0), 2.0, 1, 1.0), Late).Num()));
	}

	// T-STRK-008：右利きは右（ネットの方を向いて）がフォア、左利きは逆。奥の側の選手は右が −Y。体はボールの横と後ろ
	TEST_METHOD(Sides_FollowHandednessAndCourtSide)
	{
		const FTrajectory Right = OneSample(FVector(-10.0, 1.5, 1.0), 2.0, 1, 1.0);
		const TArray<FHitCandidate> RightHanded = Find(Right, Player(Home));
		ASSERT_THAT(IsTrue(RightHanded.Num() == 1 && RightHanded[0].Side == EStrokeSide::Forehand));
		ASSERT_THAT(IsTrue(RightHanded[0].BodyTarget.Equals(FVector2D(-10.38, 0.68), 1e-9)));  // フォア 0.82 m

		const TArray<FHitCandidate> LeftHanded = Find(Right, Player(Home, -1.0, ETennisHandedness::Left));
		ASSERT_THAT(IsTrue(LeftHanded.Num() == 1 && LeftHanded[0].Side == EStrokeSide::Backhand));
		ASSERT_THAT(IsTrue(LeftHanded[0].BodyTarget.Equals(FVector2D(-10.38, 0.88), 1e-9)));  // 両手のバック 0.62 m

		const TArray<FHitCandidate> FarSide = Find(OneSample(FVector(10.0, -1.5, 1.0), 2.0, 1, 1.0), Player(FVector2D(11.0, 0.0), 1.0));
		ASSERT_THAT(IsTrue(FarSide.Num() == 1 && FarSide[0].Side == EStrokeSide::Forehand));
		ASSERT_THAT(IsTrue(FarSide[0].BodyTarget.Equals(FVector2D(10.38, -0.68), 1e-9)));
	}

	// 中央に近い球は、間に合えばフォアに回り込み、間に合わなければバックにする
	TEST_METHOD(CenterBall_PrefersForehandWhenThereIsTime)
	{
		const FVector2D Stand(-10.38, 0.0);
		const TArray<FHitCandidate> Plenty = Find(OneSample(FVector(-10.0, -0.25, 1.0), 2.0, 1, 1.0), Player(Stand));
		ASSERT_THAT(IsTrue(Plenty.Num() == 1 && Plenty[0].Side == EStrokeSide::Forehand));

		// 使える時間 0.45 s（準備 0.30 s を除く）：フォアは 1.07 m（間に合わない）、バックは 0.37 m（間に合う）
		const TArray<FHitCandidate> Tight = Find(OneSample(FVector(-10.0, -0.25, 1.0), 0.75, 1, 0.5), Player(Stand));
		ASSERT_THAT(IsTrue(Tight.Num() == 1 && Tight[0].Side == EStrokeSide::Backhand && !Tight[0].bStretch));
	}

	// 少し届かなければストレッチ（点数が下がる）、大きく届かなければ候補にしない
	TEST_METHOD(OutOfReach_IsStretchOrDropped)
	{
		const FVector2D Stand(-10.38, 0.0);
		// 使える時間 0.45 s で進めるのは約 0.81 m。フォアの体の目標は横へ 1.25 m（残り 0.44 m）
		const TArray<FHitCandidate> Stretch = Find(OneSample(FVector(-10.0, 2.07, 1.0), 0.75, 1, 0.5), Player(Stand));  // 2.07 − 0.82 = 1.25
		ASSERT_THAT(IsTrue(Stretch.Num() == 1 && Stretch[0].bStretch));
		const TArray<FHitCandidate> Normal = Find(OneSample(FVector(-10.0, 1.5, 1.0), 2.0, 1, 1.0), Player(Stand));
		ASSERT_THAT(IsTrue(Stretch[0].Score < Normal[0].Score));

		ASSERT_THAT(AreEqual(0, Find(OneSample(FVector(-10.0, 4.0, 1.0), 0.75, 1, 0.5), Player(Stand)).Num()));
	}

	// T-STRK-001：相手のラリーの球（ソルバで作る）に候補があり、点数の降順で、いちばん良いのは間に合うグラウンドストローク。1 回 < 0.2 ms
	TEST_METHOD(RallyBall_HasSortedCandidatesAndIsFast)
	{
		FShotRequest Request;
		Request.Origin = FVector(11.0, 0.5, 1.0);
		Request.Target = FVector(-9.0, -1.0, 0.0);
		Request.ConstraintValue = 0.9;
		Request.MinSpeed = 18.0;
		Request.MaxSpeed = 40.0;
		Request.Rpm = 2000.0;
		const FTennisShotSolver Solver{ FBallSimContext() };
		const FShotSolveResult Shot = Solver.Solve(Request);
		ASSERT_THAT(IsTrue(Shot.bSolved));
		FTrajectory Trajectory;
		FTennisBallPredictor::Predict(Shot.Launch, FBallSimContext(), FPredictOptions(), Trajectory);

		FReachQuery Q = Player(FVector2D(-11.5, 1.0));
		Q.Now = Shot.Launch.Time;
		const TArray<FHitCandidate> Out = Find(Trajectory, Q);
		ASSERT_THAT(IsTrue(Out.Num() > 10));
		for (int32 Index = 1; Index < Out.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(Out[Index - 1].Score >= Out[Index].Score));
		}
		ASSERT_THAT(IsTrue(Out[0].Kind == EHitKind::Groundstroke && !Out[0].bStretch));
		ASSERT_THAT(IsTrue(Out[0].TimeToReach + Q.PrepTime <= Out[0].Time - Q.Now));

		constexpr int32 Runs = 200;
		TArray<FHitCandidate> Scratch;
		const double Start = FPlatformTime::Seconds();
		for (int32 Run = 0; Run < Runs; ++Run)
		{
			TennisReach::FindCandidates(Trajectory, Q, FMovementParams(), FReachParams(), Scratch);
		}
		const double MeanMs = (FPlatformTime::Seconds() - Start) * 1000.0 / Runs;
		UE_LOG(LogTemp, Display, TEXT("[Reach] FindCandidates: %d samples, %d candidates, mean %.4f ms"), Trajectory.States.Num(), Out.Num(), MeanMs);
		ASSERT_THAT(IsTrue(MeanMs < 0.2));
	}

	// 数でない入力は候補なし（警告を出す）
	TEST_METHOD(InvalidQuery_GivesNoCandidates)
	{
		FReachQuery Q = Player(Home);
		Q.Now = std::numeric_limits<double>::quiet_NaN();
		ASSERT_THAT(AreEqual(0, Find(OneSample(FVector(-10.0, 1.2, 1.0), 2.0, 1, 1.0), Q).Num()));
	}
};
