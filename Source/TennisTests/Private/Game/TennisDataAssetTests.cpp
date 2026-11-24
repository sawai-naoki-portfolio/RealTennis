// W-1.9：ボールとコート面のデータアセット。既定値は DATA-02 の初期値（シミュレーションの型の既定値と同じ）。
// 置いたアセット（DA_Surface_*・DA_Ball_*）の校正も確かめる（5 面の CPR・4 種類の球のドロップテスト）。
#include "CQTest.h"
#include "Calibration/CourtCalibration.h"
#include "Data/BallSpecData.h"
#include "Data/CourtSurfaceData.h"
#include "TennisDataAssetTestHelpers.h"
#include "Algo/Find.h"
#include "UObject/Package.h"

TEST_CLASS(DataAssets, "Tennis.Game.Data")
{
	// 新しく作ったデータの既定値は、シミュレーションの型の既定値と同じ（DRY：既定値の置き場所は型の 1 か所）
	TEST_METHOD(Defaults_MatchSimulationTypes)
	{
		const UCourtSurfaceData* Surface = NewObject<UCourtSurfaceData>(GetTransientPackage());
		const FSurfaceParams Expected;
		ASSERT_THAT(IsNear(Expected.RestitutionRef, Surface->Params.RestitutionRef, 1e-12));
		ASSERT_THAT(IsNear(Expected.Friction, Surface->Params.Friction, 1e-12));

		const UBallSpecData* Ball = NewObject<UBallSpecData>(GetTransientPackage());
		ASSERT_THAT(IsNear(FBallParams::Type2RestitutionHard, Ball->Params.RestitutionHard, 1e-12));
	}

	// W-1.9「5 面を校正」：置いた面のアセットは、どれも仮想 CPR 試験の値が目標の ±2 に入る（T-BALL-004）
	TEST_METHOD(SurfaceAssets_MeetTheirTargetCpr)
	{
		const TArray<const UCourtSurfaceData*> Surfaces = TennisTest::LoadAllDataAssets<UCourtSurfaceData>();
		ASSERT_THAT(IsTrue(Surfaces.Num() >= 5));
		for (const UCourtSurfaceData* Surface : Surfaces)
		{
			const FCprResult Result = TennisCourtCalibration::VirtualCprTest(Surface->Params, FBallParams());
			ASSERT_THAT(IsNear(Surface->TargetCpr, Result.Cpr, UCourtSurfaceData::CprTolerance, *Surface->GetName()));
		}
	}

	// 置いた球のアセットは、ITF Appendix I のドロップテストの範囲に入る（T-BALL-001〜003。高地球は標高 1,500 m の空気）
	TEST_METHOD(BallAssets_PassTheDropTest)
	{
		struct FRange { const TCHAR* Asset; double Min, Max, AltitudeM, TempC; };
		const FRange Ranges[] = {
			{ TEXT("DA_Ball_Type1"),        138.0, 151.0,    0.0, 20.0 },
			{ TEXT("DA_Ball_Type2"),        135.0, 147.0,    0.0, 20.0 },
			{ TEXT("DA_Ball_Type3"),        135.0, 147.0,    0.0, 20.0 },
			{ TEXT("DA_Ball_HighAltitude"), 122.0, 135.0, 1500.0, 5.25 },
		};
		const TArray<const UBallSpecData*> Balls = TennisTest::LoadAllDataAssets<UBallSpecData>();
		ASSERT_THAT(AreEqual(UE_ARRAY_COUNT(Ranges), Balls.Num()));
		for (const UBallSpecData* Ball : Balls)
		{
			const FRange* Range = Algo::FindByPredicate(Ranges, [Ball](const FRange& R) { return Ball->GetName() == R.Asset; });
			ASSERT_THAT(IsTrue(Range != nullptr, *Ball->GetName()));
			FMatchConditions Conditions;
			Conditions.AltitudeM = Range->AltitudeM;
			Conditions.TemperatureC = Range->TempC;
			const double Rebound = TennisCourtCalibration::DropTestReboundCm(Ball->Params, Conditions.ComputeAir());
			ASSERT_THAT(IsTrue(Rebound >= Range->Min && Rebound <= Range->Max, *Ball->GetName()));
		}
	}

	// 既定の面（ハードの中速）は、目標の CPR の ±2 に入る
	TEST_METHOD(DefaultSurface_IsWithinItsTargetCpr)
	{
		const UCourtSurfaceData* Surface = NewObject<UCourtSurfaceData>(GetTransientPackage());
		const FCprResult Result = TennisCourtCalibration::VirtualCprTest(Surface->Params, FBallParams());
		ASSERT_THAT(IsNear(Surface->TargetCpr, Result.Cpr, UCourtSurfaceData::CprTolerance));
	}
};
