// W-1.4（3）：ネットポストとシングルススティック。T-BALL-034（DD-01 §5.1・§5.4・§12）。
#include "CQTest.h"
#include "TennisConstants.h"
#include "TennisTestHelpers.h"

namespace
{
	TArray<FBallEvent> FlyToFirstBounce(const FVector& Pos, const FVector& Vel)
	{
		FBallState State;
		State.Pos = Pos;
		State.Vel = Vel;
		return TennisTest::FlyToFirstBounce(State);
	}

	using TennisTest::FirstOf;
}

TEST_CLASS(NetPost, "Tennis.Sim.Ball")
{
	// T-BALL-034：シングルススティックへの直撃で反射する（反発 0.5）。ポストも同じ。どちらも手前に落ちる
	TEST_METHOD(DirectHitOnStickOrPost_Reflects)
	{
		struct FCase { const TCHAR* Name; double Y; ENetPart Part; };
		const FCase Cases[] = {
			{ TEXT("stick+"), TennisConst::SinglesStickY, ENetPart::SinglesStick },
			{ TEXT("post-"), -TennisConst::NetPostY, ENetPart::Post },
		};
		for (const FCase& Case : Cases)
		{
			const TArray<FBallEvent> Events = FlyToFirstBounce(FVector(-2.0, Case.Y, 0.6), FVector(20.0, 0.0, 1.0));
			const FBallEvent* Hit = FirstOf(Events, EBallEventType::PostHit);
			ASSERT_THAT(IsTrue(Hit != nullptr && Hit->NetPart == Case.Part, Case.Name));
			ASSERT_THAT(IsTrue(Hit->Vel.X < 0.0, Case.Name));
			ASSERT_THAT(IsTrue(FirstOf(Events, EBallEventType::NetCross) == nullptr, Case.Name));
			const FBallEvent* Bounce = FirstOf(Events, EBallEventType::Bounce);
			ASSERT_THAT(IsTrue(Bounce != nullptr && Bounce->Pos.X < 0.0, Case.Name));
		}
	}

	// かすった球は横へそれる（法線は円柱の軸から外向き）
	TEST_METHOD(GlancingHitOnStick_DeflectsSideways)
	{
		const TArray<FBallEvent> Events = FlyToFirstBounce(FVector(-2.0, TennisConst::SinglesStickY + 0.05, 0.6), FVector(20.0, 0.0, 1.0));
		const FBallEvent* Hit = FirstOf(Events, EBallEventType::PostHit);
		ASSERT_THAT(IsTrue(Hit != nullptr && Hit->NetPart == ENetPart::SinglesStick));
		ASSERT_THAT(IsTrue(Hit->Vel.Y > 0.5));
	}

	// スティックの上端より上を通る球は当たらない
	TEST_METHOD(BallAboveStickTop_DoesNotHitIt)
	{
		const TArray<FBallEvent> Events = FlyToFirstBounce(FVector(-1.0, TennisConst::SinglesStickY, 1.3), FVector(25.0, 0.0, 0.0));
		ASSERT_THAT(IsTrue(FirstOf(Events, EBallEventType::PostHit) == nullptr));
		ASSERT_THAT(IsTrue(FirstOf(Events, EBallEventType::NetCross) != nullptr));
	}

	// 速い球（60 m/s）でも、ポストをすり抜けない
	TEST_METHOD(FastBallAtPost_DoesNotTunnel)
	{
		const TArray<FBallEvent> Events = FlyToFirstBounce(FVector(-1.0, TennisConst::NetPostY, 0.8), FVector(60.0, 0.0, 0.0));
		const FBallEvent* Hit = FirstOf(Events, EBallEventType::PostHit);
		ASSERT_THAT(IsTrue(Hit != nullptr && Hit->NetPart == ENetPart::Post));
		const FBallEvent* Bounce = FirstOf(Events, EBallEventType::Bounce);
		ASSERT_THAT(IsTrue(Bounce != nullptr && Bounce->Pos.X < 0.0));
	}
};
