// W-1.4（1）：ネットの形と、ネットの面を越えるときの扱い。T-BALL-030・032・033（DD-01 §5・§12）。
#include "CQTest.h"
#include "Court/CourtGeometry.h"
#include "TennisConstants.h"
#include "TennisTestHelpers.h"

using TennisTest::CountOf;
using TennisTest::FirstOf;

TEST_CLASS(Net, "Tennis.Sim.Ball")
{
	// DD-01 §5.1：中央 0.914 m からスティック 1.07 m まで直線、スティックからポストまでは 1.07 m
	TEST_METHOD(NetHeight_FollowsRule1)
	{
		ASSERT_THAT(IsNear(0.914, TennisCourtGeometry::NetHeightAt(0.0), 1e-12));
		ASSERT_THAT(IsNear(1.07, TennisCourtGeometry::NetHeightAt(TennisConst::SinglesStickY), 1e-12));
		ASSERT_THAT(IsNear(1.07, TennisCourtGeometry::NetHeightAt(-6.0), 1e-12));
		ASSERT_THAT(IsNear(0.914 + 0.156 * 0.5, TennisCourtGeometry::NetHeightAt(-0.5 * TennisConst::SinglesStickY), 1e-12));
	}

	// 境界：本体はスティックまで、ネットの面はポストまで（Rule 25 c・d）
	TEST_METHOD(NetExtent_StopsAtStickAndPost)
	{
		ASSERT_THAT(IsTrue(TennisCourtGeometry::HasNetBodyAt(TennisConst::SinglesStickY)));
		ASSERT_THAT(IsFalse(TennisCourtGeometry::HasNetBodyAt(TennisConst::SinglesStickY + 0.001)));
		ASSERT_THAT(IsTrue(TennisCourtGeometry::HasNetAt(-TennisConst::NetPostY)));
		ASSERT_THAT(IsFalse(TennisCourtGeometry::HasNetAt(-TennisConst::NetPostY - 0.001)));
	}

	// T-BALL-030：高さ 0.5 m で X 方向に打った球は網に当たり、自分のコートに落ちる（どちらの側から打っても）
	TEST_METHOD(BallIntoNetBody_FallsOnOwnSide)
	{
		for (const double Dir : { 1.0, -1.0 })
		{
			FBallState State;
			State.Pos = FVector(-Dir * 1.0, 0.0, 0.5);
			State.Vel = FVector(Dir * 20.0, 0.0, 0.0);
			const TArray<FBallEvent> Events = TennisTest::FlyToFirstBounce(State);

			ASSERT_THAT(AreEqual(1, CountOf(Events, EBallEventType::NetTouch)));
			ASSERT_THAT(AreEqual(0, CountOf(Events, EBallEventType::NetCross)));
			ASSERT_THAT(IsTrue(FirstOf(Events, EBallEventType::NetTouch)->NetPart == ENetPart::Body));
			ASSERT_THAT(IsTrue(State.bNetTouched));
			const FBallEvent* Bounce = FirstOf(Events, EBallEventType::Bounce);
			ASSERT_THAT(IsTrue(Bounce != nullptr && Bounce->Pos.X * Dir < 0.0));
		}
	}

	// ネットの上を越える球：NetCross のイベントに、ボールの底とネットの上端の差（余裕）が入る。イベントは時刻の順
	TEST_METHOD(BallOverNet_ReportsClearance)
	{
		FBallState State;
		State.Pos = FVector(-5.0, 1.0, 1.5);
		State.Vel = FVector(25.0, 0.0, 1.0);
		const TArray<FBallEvent> Events = TennisTest::FlyToFirstBounce(State);

		ASSERT_THAT(AreEqual(0, CountOf(Events, EBallEventType::NetTouch)));
		const FBallEvent* Cross = FirstOf(Events, EBallEventType::NetCross);
		ASSERT_THAT(IsTrue(Cross != nullptr));
		ASSERT_THAT(IsNear(0.0, Cross->Pos.X, 1e-12));
		const double Expected = Cross->Pos.Z - FBallParams().Radius - TennisCourtGeometry::NetHeightAt(Cross->Pos.Y);
		ASSERT_THAT(IsNear(Expected, Cross->NetClearance, 1e-12));
		ASSERT_THAT(IsTrue(Cross->NetClearance > 0.3));
		const FBallEvent* Bounce = FirstOf(Events, EBallEventType::Bounce);
		ASSERT_THAT(IsTrue(Bounce != nullptr && Bounce->Pos.X > 0.0 && Cross->Time < Bounce->Time));
		ASSERT_THAT(IsFalse(State.bNetTouched));
	}

	// T-BALL-032：ポストの外を回る球は、ネットの判定がなく、相手のコートに落ちる（Rule 25 c）
	TEST_METHOD(BallAroundThePost_HasNoNetEvent)
	{
		FBallState State;
		State.Pos = FVector(-2.0, 7.0, 0.5);
		State.Vel = FVector(20.0, 0.0, 2.0);
		const TArray<FBallEvent> Events = TennisTest::FlyToFirstBounce(State);

		ASSERT_THAT(AreEqual(0, CountOf(Events, EBallEventType::NetTouch) + CountOf(Events, EBallEventType::NetCross)));
		const FBallEvent* Bounce = FirstOf(Events, EBallEventType::Bounce);
		ASSERT_THAT(IsTrue(Bounce != nullptr && Bounce->Pos.X > 0.0));
	}

	// T-BALL-033：スティックとポストの間で白帯の下を通る球は、ネットに触れない（Rule 25 d）
	TEST_METHOD(BallUnderCordBetweenStickAndPost_DoesNotTouchNet)
	{
		FBallState State;
		State.Pos = FVector(-2.0, 5.7, 0.6);
		State.Vel = FVector(20.0, 0.0, 1.0);
		const TArray<FBallEvent> Events = TennisTest::FlyToFirstBounce(State);

		ASSERT_THAT(AreEqual(0, CountOf(Events, EBallEventType::NetTouch)));
		const FBallEvent* Cross = FirstOf(Events, EBallEventType::NetCross);
		ASSERT_THAT(IsTrue(Cross != nullptr && Cross->NetClearance < 0.0));
		const FBallEvent* Bounce = FirstOf(Events, EBallEventType::Bounce);
		ASSERT_THAT(IsTrue(Bounce != nullptr && Bounce->Pos.X > 0.0));
		ASSERT_THAT(IsFalse(State.bNetTouched));
	}
};
