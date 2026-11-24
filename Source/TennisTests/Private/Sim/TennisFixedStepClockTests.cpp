// W-1.6：フレームの時間から固定ステップへの変換。T-BALL-020（DD-01 §3.3・§12）。
#include "CQTest.h"
#include "Ball/TennisBallPhysics.h"
#include "FixedStepClock.h"
#include "TennisConstants.h"
#include "TennisTestHelpers.h"

#include <limits>

namespace
{
	// fps の一定のフレームで、最初のバウンドまで本番と同じように進め、その位置を返す
	FVector FirstBounceAtFrameRate(double Fps)
	{
		const FBallSimContext Ctx;
		FBallState State = TennisTest::RallyShot();

		FFixedStepClock Clock;
		TArray<FBallEvent> Events;
		for (int32 Frame = 0; Frame < 10 * 144; ++Frame)
		{
			const int32 Steps = Clock.Advance(1.0 / Fps);
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				TennisBallPhysics::Step(State, TennisConst::SimDt, Ctx, Events);
			}
			if (const FBallEvent* Bounce = TennisTest::FirstOf(Events, EBallEventType::Bounce))
			{
				return Bounce->Pos;
			}
		}
		return FVector(TNumericLimits<double>::Max());
	}
}

TEST_CLASS(FixedStepClock, "Tennis.Sim.Clock")
{
	// T-BALL-020：30／60／144 fps で、同じ打ち出しの着地点が 1 mm 以内で一致する
	TEST_METHOD(LandingPoint_IsIndependentOfFrameRate)
	{
		const FVector At60 = FirstBounceAtFrameRate(60.0);
		ASSERT_THAT(IsTrue(At60.X > 0.0 && At60.X < TennisConst::HalfLength + 3.0));
		for (const double Fps : { 30.0, 144.0 })
		{
			ASSERT_THAT(IsTrue(FVector::Dist(At60, FirstBounceAtFrameRate(Fps)) < 0.001));
		}
	}

	// 1 秒分のフレームで、ちょうど 240 ステップ（丸めの誤差で 1 つずれてもよい）
	TEST_METHOD(OneSecondOfFrames_Is240Steps)
	{
		for (const double Fps : { 30.0, 60.0, 144.0 })
		{
			FFixedStepClock Clock;
			int32 Steps = 0;
			for (int32 Frame = 0; Frame < static_cast<int32>(Fps); ++Frame)
			{
				Steps += Clock.Advance(1.0 / Fps);
			}
			ASSERT_THAT(IsTrue(FMath::Abs(Steps - 240) <= 1));
			ASSERT_THAT(IsTrue(Clock.Alpha() >= 0.0 && Clock.Alpha() < 1.0));
		}
	}

	// 長いフレームは 0.1 s に切り詰め、時間の倍率をかける（0.1 s ≒ 24 ステップ。1/240 は 2 進で割り切れないので、同じ時間との比較で確かめる）
	TEST_METHOD(LongFrame_IsClampedThenScaled)
	{
		const int32 TenthOfSecond = FFixedStepClock().Advance(FFixedStepClock::MaxFrameTime);
		ASSERT_THAT(IsTrue(TenthOfSecond >= 23 && TenthOfSecond <= 24));
		ASSERT_THAT(AreEqual(TenthOfSecond, FFixedStepClock().Advance(5.0)));
		ASSERT_THAT(AreEqual(FFixedStepClock().Advance(0.05), FFixedStepClock().Advance(0.1, 0.5)));
		ASSERT_THAT(AreEqual(FFixedStepClock().Advance(0.05), FFixedStepClock().Advance(5.0, 0.5)));
	}

	// 堅牢性：負・NaN のフレームの時間と倍率は 0 として扱う
	TEST_METHOD(InvalidInput_AdvancesNothing)
	{
		FFixedStepClock Clock;
		const double NaN = std::numeric_limits<double>::quiet_NaN();
		ASSERT_THAT(AreEqual(0, Clock.Advance(-1.0)));
		ASSERT_THAT(AreEqual(0, Clock.Advance(NaN)));
		ASSERT_THAT(AreEqual(0, Clock.Advance(0.1, -2.0)));
		ASSERT_THAT(AreEqual(0, Clock.Advance(0.1, NaN)));
		ASSERT_THAT(IsNear(0.0, Clock.Alpha(), 1e-12));
	}
};
