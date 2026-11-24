// W-1.9：校正の試験の関数（Calibration/CourtCalibration.h）。CPR の値とドロップテストは GroundBounce・BallStep のテストで確かめる。
#include "CQTest.h"
#include "Calibration/CourtCalibration.h"

TEST_CLASS(CourtCalibration, "Tennis.Sim.Calibration")
{
	// ITF の区分の境界（30・35・40・45）
	TEST_METHOD(CprCategory_Boundaries)
	{
		ASSERT_THAT(AreEqual(1, TennisCourtCalibration::CprCategory(29.999)));
		ASSERT_THAT(AreEqual(2, TennisCourtCalibration::CprCategory(30.0)));
		ASSERT_THAT(AreEqual(2, TennisCourtCalibration::CprCategory(34.999)));
		ASSERT_THAT(AreEqual(3, TennisCourtCalibration::CprCategory(35.0)));
		ASSERT_THAT(AreEqual(4, TennisCourtCalibration::CprCategory(40.0)));
		ASSERT_THAT(AreEqual(5, TennisCourtCalibration::CprCategory(45.0)));
	}

	// 仮想 CPR 試験の見かけの値は、物理として成り立つ範囲（0 < e_app < 1、μ_app ≥ 0）
	TEST_METHOD(VirtualCprTest_ApparentValuesArePhysical)
	{
		const FCprResult Result = TennisCourtCalibration::VirtualCprTest(FSurfaceParams(), FBallParams());
		ASSERT_THAT(IsTrue(Result.ApparentRestitution > 0.0 && Result.ApparentRestitution < 1.0));
		ASSERT_THAT(IsTrue(Result.ApparentFriction >= 0.0));
		ASSERT_THAT(AreEqual(TennisCourtCalibration::CprCategory(Result.Cpr), Result.Category));
	}
};
