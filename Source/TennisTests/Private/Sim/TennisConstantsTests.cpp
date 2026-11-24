// T-CRT-001 の一部：ITF Rules of Tennis 2026 の寸法と、定数どうしの関係を確かめる。
#include "CQTest.h"
#include "TennisConstants.h"

namespace
{
	constexpr double Exact = 1e-9;  // 定数は計算で決まるので、丸めの誤差だけを許す
}

TEST_CLASS(TennisConstants, "Tennis.Sim.Court")
{
	TEST_METHOD(CourtSize_MatchesItfRule1)
	{
		ASSERT_THAT(IsNear(23.77, 2.0 * TennisConst::HalfLength, Exact));
		ASSERT_THAT(IsNear(8.23, 2.0 * TennisConst::HalfWidthSingles, Exact));
		ASSERT_THAT(IsNear(10.97, 2.0 * TennisConst::HalfWidthDoubles, Exact));
		ASSERT_THAT(IsNear(6.40, TennisConst::ServiceLineX, Exact));
	}

	TEST_METHOD(PostsAndSticks_Are0914mOutsideTheirSidelines)
	{
		ASSERT_THAT(IsNear(5.029, TennisConst::SinglesStickY, 1e-6));
		ASSERT_THAT(IsNear(6.399, TennisConst::NetPostY, 1e-6));
		ASSERT_THAT(IsNear(1.095, TennisConst::PostHeight, 1e-9));
	}

	TEST_METHOD(Net_IsLowerAtCenterThanAtPosts)
	{
		ASSERT_THAT(IsNear(0.914, TennisConst::NetHeightCenter, Exact));
		ASSERT_THAT(IsNear(1.07, TennisConst::NetHeightPost, Exact));
		ASSERT_THAT(IsTrue(TennisConst::NetHeightCenter < TennisConst::NetHeightPost));
	}

	TEST_METHOD(SimStep_Is240Hz)
	{
		ASSERT_THAT(IsNear(1.0, TennisConst::SimDt * TennisConst::SimHz, Exact));
	}
};
