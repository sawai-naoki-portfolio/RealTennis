// 単位の変換（ARCH-03 U-3）。
#include "CQTest.h"
#include "TennisUnits.h"

TEST_CLASS(TennisUnitConversion, "Tennis.Sim.Units")
{
	// 60 rpm は 1 秒に 1 回転（2π rad/s）。トップスピンの 3,000 rpm は約 314 rad/s
	TEST_METHOD(RpmToRadPerSec_ConvertsRevolutionsPerMinute)
	{
		ASSERT_THAT(IsNear(UE_DOUBLE_TWO_PI, TennisUnits::RpmToRadPerSec(60.0), 1e-12));
		ASSERT_THAT(IsNear(0.0, TennisUnits::RpmToRadPerSec(0.0), 1e-12));
		ASSERT_THAT(IsNear(314.159265, TennisUnits::RpmToRadPerSec(3000.0), 1e-6));
		ASSERT_THAT(IsNear(-UE_DOUBLE_TWO_PI, TennisUnits::RpmToRadPerSec(-60.0), 1e-12));
	}
};
