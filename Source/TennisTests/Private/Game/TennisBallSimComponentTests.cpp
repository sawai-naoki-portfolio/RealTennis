// W-1.6：ボールのシミュレーションのコンポーネント。T-BALL-022（DD-01 §3.4・§12）。ワールドなしで確かめられる部分と、
// T-BALL-024（ステップの通し番号。DD-01 §3.3）。
#include "CQTest.h"
#include "Ball/TennisBall.h"
#include "Ball/TennisBallPhysics.h"
#include "Ball/TennisBallSimComponent.h"
#include "Components/ActorTestSpawner.h"
#include "Core/TennisDebugSettings.h"
#include "FixedStepClock.h"
#include "TennisConstants.h"
#include "TennisGameTestHelpers.h"
#include "TennisTestHelpers.h"
#include "UObject/Package.h"

namespace
{
	// 共通のラリーの球に、打球の通し番号 3 を付ける
	FBallState RallyShot(double Time)
	{
		FBallState State = TennisTest::RallyShot(Time);
		State.Seq = 3;
		return State;
	}

	FBallState RestingBall(double Time)
	{
		FBallState State;
		State.Pos = FVector(-8.0, 0.0, FBallParams().Radius);
		State.Time = Time;
		return State;
	}
}

TEST_CLASS(BallSimComponent, "Tennis.Game.Ball")
{
	// 打ち出すと、その状態から予測を作る（予測と本番は同じ物理）
	TEST_METHOD(Launch_SetsStateAndPrediction)
	{
		UTennisBallSimComponent* Sim = NewObject<UTennisBallSimComponent>(GetTransientPackage());
		Sim->Launch(RallyShot(0.0));
		ASSERT_THAT(IsTrue(Sim->GetState().Pos == RallyShot(0.0).Pos));

		FTrajectory Expected;
		FTennisBallPredictor::Predict(RallyShot(0.0), FBallSimContext(), FPredictOptions(), Expected);
		ASSERT_THAT(AreEqual(Expected.States.Num(), Sim->GetPrediction().States.Num()));
		ASSERT_THAT(IsTrue(Expected.States.Last().Pos == Sim->GetPrediction().States.Last().Pos));
		ASSERT_THAT(AreEqual(3, Sim->GetPrediction().Seq));

		FBallState At;
		ASSERT_THAT(IsTrue(Sim->GetStateAt(0.5, At)));
		ASSERT_THAT(IsFalse(Sim->GetStateAt(-1.0, At)));
	}

	// T-BALL-022：過去の時刻の打ち出しは今まで早送りし、最初からその時刻に打ち出して進めたものと一致する。
	// 早送りの間のイベントも配信する
	TEST_METHOD(LaunchInThePast_FastForwardsToNow)
	{
		UTennisBallSimComponent* Sim = NewObject<UTennisBallSimComponent>(GetTransientPackage());
		Sim->Launch(RestingBall(1.2));  // 今のシミュレーションの時刻を 1.2 s にする

		int32 Delivered = 0;
		Sim->OnBallEvent.AddLambda([&Delivered](const FBallEvent&) { ++Delivered; });
		Sim->Launch(RallyShot(0.2));  // 1 s 遅れて届いた打ち出し（その間にネットを越え、バウンドもする）

		FBallState Expected = RallyShot(0.2);
		TArray<FBallEvent> ExpectedEvents;
		ASSERT_THAT(IsTrue(TennisBallPhysics::AdvanceTo(Expected, 1.2, FBallSimContext(), ExpectedEvents)));
		ASSERT_THAT(IsTrue(Sim->GetState().Time == 1.2));
		ASSERT_THAT(IsTrue(Sim->GetState().Pos == Expected.Pos && Sim->GetState().Vel == Expected.Vel));
		ASSERT_THAT(IsTrue(ExpectedEvents.Num() >= 2));
		ASSERT_THAT(AreEqual(ExpectedEvents.Num(), Delivered));
	}

	// 今より後の時刻の打ち出しは、早送りせずにそのまま使う
	TEST_METHOD(LaunchInTheFuture_IsUsedAsIs)
	{
		UTennisBallSimComponent* Sim = NewObject<UTennisBallSimComponent>(GetTransientPackage());
		Sim->Launch(RestingBall(1.0));
		Sim->Launch(RallyShot(2.0));
		ASSERT_THAT(IsTrue(Sim->GetState().Time == 2.0 && Sim->GetState().Pos == RallyShot(2.0).Pos));
	}
};

// Tick を呼ぶテスト（コンポーネントを登録するため、ワールドに置く。BeginPlay は呼ばないので、自動では Tick しない）
TEST_CLASS(BallSimComponentTick, "Tennis.Game.Ball")
{
	FActorTestSpawner Spawner;
	float SavedTimeScale = 1.0f;

	BEFORE_EACH()
	{
		SavedTimeScale = GetDefault<UTennisDebugSettings>()->GetTimeScale();
	}

	AFTER_EACH()
	{
		TennisTest::SetTimeScale(SavedTimeScale);
	}

	// T-BALL-024：ステップの通し番号は、飛んでいない間も数え、状態と時刻は変えない。打ち出しは番号を変えない。飛んでいる間は、フレームごとの
	// 番号の差 × SimDt だけ時刻が進む（不ぞろいのフレーム・時間の倍率によらない。ストロークはこの差だけ進む：DD-03 §6.1）。
	// 番号は、同じフレームの列を与えた時計と同じ値になる
	TEST_METHOD(StepCount_RunsOutOfFlightAndMatchesTheTimeInFlight)
	{
		for (const float Scale : { 1.0f, 0.5f })
		{
			TennisTest::SetTimeScale(Scale);
			const FString Label = FString::Printf(TEXT("time scale %.2f"), Scale);
			UTennisBallSimComponent* Sim = Spawner.SpawnActor<ATennisBall>().GetSim();
			ASSERT_THAT(IsTrue(Sim != nullptr && Sim->GetStepCount() == 0, *Label));
			FFixedStepClock Reference;  // 同じフレームの列を与えた時計（番号の正しい値）
			int64 Expected = 0;
			const auto Tick = [&](float FrameTime)
			{
				Sim->TickComponent(FrameTime, LEVELTICK_All, nullptr);
				Expected += Reference.Advance(FrameTime, Scale);
			};

			const FBallState Before = Sim->GetState();
			for (int32 Frame = 0; Frame < 6; ++Frame)
			{
				Tick(1.0f / 60.0f);
			}
			ASSERT_THAT(IsTrue(Expected > 0 && Sim->GetStepCount() == Expected, *Label));  // 飛んでいない間も数える
			ASSERT_THAT(IsTrue(Sim->GetState().Time == Before.Time && Sim->GetState().Pos == Before.Pos, *Label));

			Sim->Launch(RallyShot(Sim->GetState().Time));
			ASSERT_THAT(IsTrue(Sim->GetStepCount() == Expected, *Label));  // 打ち出しは番号を変えない
			for (const float FrameTime : { 1.0f / 60.0f, 1.0f / 144.0f, 1.0f / 30.0f, 0.0f, 1.0f / 90.0f, 1.0f / 60.0f })
			{
				const int64 CountBefore = Sim->GetStepCount();
				const double TimeBefore = Sim->GetState().Time;
				Tick(FrameTime);
				ASSERT_THAT(IsTrue(Sim->GetStepCount() == Expected, *Label));
				ASSERT_THAT(IsNear(static_cast<double>(Sim->GetStepCount() - CountBefore) * TennisConst::SimDt, Sim->GetState().Time - TimeBefore, 1e-12, *Label));
			}
		}
	}
};
