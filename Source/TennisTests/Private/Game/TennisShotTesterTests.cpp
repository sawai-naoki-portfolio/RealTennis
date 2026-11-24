// W-2.4：打球ツール（FR-DBG-003）。ワールドなしで確かめられる部分：クリックの半直線と地面の交点、狙った地点への打ち出し。
#include "CQTest.h"
#include "Ball/TennisBallSimComponent.h"
#include "Debug/TennisShotTester.h"
#include "Shot/ShotTypeParams.h"
#include "UObject/Package.h"

TEST_CLASS(ShotTester, "Tennis.Game.Debug")
{
	// 地面へ向かう半直線は Z = 0 の点。上向き・水平・地面の下からの半直線は値なし
	TEST_METHOD(GroundPointOnRay_HitsTheGroundOnlyWhenPointingDown)
	{
		const TOptional<FVector> Hit = ATennisShotTester::GroundPointOnRay(FVector(-20.0, 1.0, 10.0), FVector(2.0, 0.5, -1.0));
		ASSERT_THAT(IsTrue(Hit.IsSet()));
		ASSERT_THAT(IsTrue(Hit->Equals(FVector(0.0, 6.0, 0.0), 1e-9)));
		ASSERT_THAT(IsFalse(ATennisShotTester::GroundPointOnRay(FVector(0.0, 0.0, 10.0), FVector(1.0, 0.0, 0.0)).IsSet()));
		ASSERT_THAT(IsFalse(ATennisShotTester::GroundPointOnRay(FVector(0.0, 0.0, 10.0), FVector(0.0, 0.0, 1.0)).IsSet()));
		ASSERT_THAT(IsFalse(ATennisShotTester::GroundPointOnRay(FVector(0.0, 0.0, -1.0), FVector(0.0, 0.0, -1.0)).IsSet()));
	}

	// 狙った地点へ打ち出し、そのボールの予測（本番と同じ物理）の最初のバウンドが 5 cm 以内（M2 の完了の条件）
	TEST_METHOD(Shoot_LaunchesTheBallSoItLandsOnTheTarget)
	{
		UTennisBallSimComponent* Sim = NewObject<UTennisBallSimComponent>(GetTransientPackage());
		FShotTypeParams Topspin;
		Topspin.MinSpeed = 22.0;
		Topspin.MaxSpeed = 38.0;
		Topspin.BaseRpm = 2300.0;
		Topspin.ConstraintAtCharge0 = 1.40;
		Topspin.ConstraintAtCharge1 = 0.60;
		Topspin.InitialElevationDeg = 9.0;
		const FVector Target(8.5, -2.5, 0.0);

		const FShotSolveResult Result = ATennisShotTester::Shoot(*Sim, Topspin, FVector(-11.0, 0.5, 1.0), Target, 0.5);
		ASSERT_THAT(IsTrue(Result.bSolved));
		ASSERT_THAT(IsTrue(Sim->GetState().Vel == Result.Launch.Vel));

		const FBallEvent* Bounce = Sim->GetPrediction().FirstBounce();
		ASSERT_THAT(IsTrue(Bounce != nullptr));
		ASSERT_THAT(IsTrue(FVector::Dist2D(Bounce->Pos, Target) < 0.05));
	}
};
