// W-1.4（2）：白帯（ネットコード）との衝突。T-BALL-031・035（DD-01 §5.2・§12）。
#include "CQTest.h"
#include "TennisConstants.h"
#include "TennisTestHelpers.h"

namespace
{
	struct FFlight
	{
		FBallState Final;
		TArray<FBallEvent> Events;
	};

	FFlight FlyToFirstBounce(const FVector& Pos, const FVector& Vel)
	{
		FFlight Flight;
		Flight.Final.Pos = Pos;
		Flight.Final.Vel = Vel;
		Flight.Events = TennisTest::FlyToFirstBounce(Flight.Final);
		return Flight;
	}

	bool TouchedCord(const FFlight& Flight)
	{
		return Flight.Events.ContainsByPredicate([](const FBallEvent& E)
		{
			return E.Type == EBallEventType::NetTouch && E.NetPart == ENetPart::Cord;
		});
	}

	const FBallEvent* FirstBounce(const FFlight& Flight)
	{
		return TennisTest::FirstOf(Flight.Events, EBallEventType::Bounce);
	}
}

TEST_CLASS(NetCord, "Tennis.Sim.Ball")
{
	// T-BALL-031：白帯に当てる高さを少しずつ変えると、越える球と戻る球の両方が起きる。同じ条件では常に同じ
	TEST_METHOD(CordBalls_GoOverOrFallBack_Deterministically)
	{
		int32 Over = 0;
		int32 Back = 0;
		for (double Z0 = 1.06; Z0 <= 1.17; Z0 += 0.0025)
		{
			const FFlight Flight = FlyToFirstBounce(FVector(-3.0, 0.0, Z0), FVector(15.0, 0.0, 0.0));
			const FBallEvent* Bounce = FirstBounce(Flight);
			if (TouchedCord(Flight) && Bounce != nullptr)
			{
				(Bounce->Pos.X > 0.0 ? Over : Back)++;
				ASSERT_THAT(IsTrue(Flight.Final.bNetTouched));
			}
		}
		ASSERT_THAT(IsTrue(Over > 0 && Back > 0));

		const FFlight A = FlyToFirstBounce(FVector(-3.0, 0.0, 1.11), FVector(15.0, 0.0, 0.0));
		const FFlight B = FlyToFirstBounce(FVector(-3.0, 0.0, 1.11), FVector(15.0, 0.0, 0.0));
		ASSERT_THAT(IsTrue(A.Final.Pos == B.Final.Pos && A.Final.Vel == B.Final.Vel && A.Events.Num() == B.Events.Num()));
	}

	// 速い球（60 m/s。1 ステップで 25 cm 進む）でも、白帯をすり抜けずに跳ね返る
	TEST_METHOD(FastBallAtCord_DoesNotTunnel)
	{
		const FFlight Flight = FlyToFirstBounce(FVector(-1.0, 0.0, 0.914), FVector(60.0, 0.0, 0.0));
		ASSERT_THAT(IsTrue(TouchedCord(Flight)));
		ASSERT_THAT(AreEqual(0, TennisTest::CountOf(Flight.Events, EBallEventType::NetCross)));
		const FBallEvent* Bounce = FirstBounce(Flight);
		ASSERT_THAT(IsTrue(Bounce != nullptr && Bounce->Pos.X < 0.0));
	}

	// 白帯はポストの内側だけ（Rule 25 c）：白帯の高さでポストの外を回る球は触れない
	TEST_METHOD(BallAroundPostAtCordHeight_DoesNotTouchCord)
	{
		const FFlight Flight = FlyToFirstBounce(FVector(-1.0, 7.0, 1.07), FVector(30.0, 0.0, 0.0));
		ASSERT_THAT(IsFalse(TouchedCord(Flight)));
		ASSERT_THAT(IsFalse(Flight.Final.bNetTouched));
	}

	// T-BALL-035：1 ステップの中の 2 回の衝突（地面 → 網）を、時刻の順に解く
	TEST_METHOD(TwoContactsInOneStep_AreResolvedInTimeOrder)
	{
		const FBallSimContext Ctx;
		FBallState State;
		State.Pos = FVector(-0.05, 0.0, Ctx.Ball.Radius + 0.01);
		State.Vel = FVector(20.0, 0.0, -5.0);
		TArray<FBallEvent> Events;
		TennisBallPhysics::Step(State, TennisConst::SimDt, Ctx, Events);

		ASSERT_THAT(AreEqual(2, Events.Num()));
		ASSERT_THAT(IsTrue(Events[0].Type == EBallEventType::Bounce));
		ASSERT_THAT(IsTrue(Events[1].Type == EBallEventType::NetTouch && Events[1].NetPart == ENetPart::Body));
		ASSERT_THAT(IsTrue(0.0 < Events[0].Time && Events[0].Time < Events[1].Time && Events[1].Time < TennisConst::SimDt));
		ASSERT_THAT(IsNear(TennisConst::SimDt, State.Time, 1e-12));
	}
};
