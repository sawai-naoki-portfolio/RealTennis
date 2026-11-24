// W-3.5、T-STRK-016：インパクトの判定（DD-03 §6.4 の 3〜5）。スイートスポットの位置が打点の選び直しの体の位置（BodyTarget）の
// ちょうど逆、利き手、T の体の位置、打てる距離の境界（FR-STRK-006）。溜めは TennisShotRequestMappingTests。
#include "CQTest.h"
#include "Stroke/TennisStrokeImpact.h"
#include "Stroke/TennisStrokeTiming.h"

#include <limits>

TEST_CLASS(StrokeImpact, "Tennis.Sim.Stroke")
{
	const FStrokeSyncParams Params;

	// 体の位置から求めたスイートスポットに、BodyTarget で体を置き直すと元の体の位置（コートの両側の向き・前と横と高さのずれ）
	TEST_METHOD(SweetSpotAt_IsTheExactInverseOfBodyTarget)
	{
		const FVector Offsets[] = {FVector(0.228, 0.850, 1.0), FVector(0.38, -0.62, 1.08), FVector(0.0, 0.0, 0.9)};
		const double Yaws[] = {0.0, 0.6, -0.6, UE_DOUBLE_PI, UE_DOUBLE_PI - 0.5, -UE_DOUBLE_PI + 0.5};
		const FVector2D Body(-11.3, 1.7);
		for (const FVector& Offset : Offsets)
		{
			for (const double Yaw : Yaws)
			{
				const FVector Spot = TennisStrokeImpact::SweetSpotAt(Body, Yaw, Offset);
				const FVector2D Back = TennisStrokeTiming::BodyTarget(FVector2D(Spot.X, Spot.Y), Yaw, FVector2D(Offset.X, Offset.Y));
				ASSERT_THAT(IsNear(Body.X, Back.X, 1e-9));
				ASSERT_THAT(IsNear(Body.Y, Back.Y, 1e-9));
				ASSERT_THAT(IsTrue(Offset.Z == Spot.Z));  // 高さは焼いた値のまま（体の位置は地面の上）
				ASSERT_THAT(IsNear(FVector2D(Offset.X, Offset.Y).Size(), (FVector2D(Spot.X, Spot.Y) - Body).Size(), 1e-9));
			}
		}
	}

	// +X を向いた右利きは、焼いた値の右（+Y）が +Y。左利きは横を反転し、前と高さはそのまま
	TEST_METHOD(SweetSpotAt_TurnsWithTheBodyAndMirrorsForTheLeftHand)
	{
		const FVector Offset(0.3, 0.8, 1.0);
		const FVector Right = TennisStrokeImpact::SweetSpotAt(FVector2D::ZeroVector, 0.0, TennisStrokeImpact::HandedOffset(Offset, ETennisHandedness::Right));
		ASSERT_THAT(IsNear(0.3, Right.X, 1e-12));
		ASSERT_THAT(IsNear(0.8, Right.Y, 1e-12));
		const FVector Left = TennisStrokeImpact::SweetSpotAt(FVector2D::ZeroVector, 0.0, TennisStrokeImpact::HandedOffset(Offset, ETennisHandedness::Left));
		ASSERT_THAT(IsNear(0.3, Left.X, 1e-12));
		ASSERT_THAT(IsNear(-0.8, Left.Y, 1e-12));
		ASSERT_THAT(IsTrue(Left.Z == 1.0));
		// 奥の選手（−X を向く）では、右は −Y
		const FVector Far = TennisStrokeImpact::SweetSpotAt(FVector2D(11.0, 0.0), UE_DOUBLE_PI, Offset);
		ASSERT_THAT(IsNear(11.0 - 0.3, Far.X, 1e-12));
		ASSERT_THAT(IsNear(-0.8, Far.Y, 1e-12));
	}

	// T の体の位置：フレームの始まりの位置 + 速度 ×（T − フレームの始まりの時刻）
	TEST_METHOD(BodyAt_MovesTheFrameStartByTheVelocityUntilT)
	{
		const FVector2D Start(-11.0, 2.0);
		const FVector2D Velocity(1.5, -6.0);
		const FVector2D At = TennisStrokeImpact::BodyAt(Start, Velocity, 3.0, 3.0 + 3.0 / 240.0);
		ASSERT_THAT(IsNear(-11.0 + 1.5 / 80.0, At.X, 1e-12));
		ASSERT_THAT(IsNear(2.0 - 6.0 / 80.0, At.Y, 1e-12));
		ASSERT_THAT(IsTrue(Start == TennisStrokeImpact::BodyAt(Start, Velocity, 3.0, 3.0)));
	}

	// 距離がちょうど判定の距離（0.25 m：DATA-02 §4.2）なら打つ。1 mm 遠ければ空振り。数でない距離は打たない
	TEST_METHOD(IsWithinReach_HitsUpToTheReachInclusive)
	{
		ASSERT_THAT(IsTrue(Params.ImpactReach == 0.25));
		ASSERT_THAT(IsTrue(TennisStrokeImpact::IsWithinReach(0.0, Params)));
		ASSERT_THAT(IsTrue(TennisStrokeImpact::IsWithinReach(0.25, Params)));
		ASSERT_THAT(IsFalse(TennisStrokeImpact::IsWithinReach(0.251, Params)));
		ASSERT_THAT(IsFalse(TennisStrokeImpact::IsWithinReach(std::numeric_limits<double>::quiet_NaN(), Params)));
		FStrokeSyncParams Broken = Params;
		Broken.ImpactReach = std::numeric_limits<double>::quiet_NaN();
		ASSERT_THAT(IsFalse(TennisStrokeImpact::IsWithinReach(0.0, Broken)));
	}
};
