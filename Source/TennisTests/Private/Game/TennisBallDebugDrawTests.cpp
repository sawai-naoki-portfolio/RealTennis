// W-1.8：デバッグの描画の点列（間引きと座標の変換）。描画そのものは目視で確かめる。
#include "CQTest.h"
#include "Ball/TennisBallDebugDraw.h"
#include "Ball/TennisBallPredictor.h"
#include "Court/TennisCourt.h"

namespace
{
	FTrajectory StraightTrajectory(int32 NumStates)
	{
		FTrajectory Trajectory;
		for (int32 Index = 0; Index < NumStates; ++Index)
		{
			FBallState& State = Trajectory.States.AddDefaulted_GetRef();
			State.Pos = FVector(Index * 0.1, 0.0, 1.0);
		}
		return Trajectory;
	}
}

TEST_CLASS(BallDebugDraw, "Tennis.Game.Ball")
{
	// Stride ごとに間引き、最後の状態は必ず入れる。位置はコートの変換でワールド（cm）にする
	TEST_METHOD(Polyline_DecimatesAndKeepsLastState)
	{
		const FTransform Court(FVector(100.0, 0.0, 0.0));
		const TArray<FVector> Points = TennisBallDebugDraw::TrajectoryPolyline(StraightTrajectory(10), Court, 4);
		ASSERT_THAT(AreEqual(4, Points.Num()));  // 0・4・8 と最後の 9
		ASSERT_THAT(IsTrue(Points[0].Equals(ATennisCourt::LocalMetersToWorld(Court, FVector(0.0, 0.0, 1.0)), 1e-9)));
		ASSERT_THAT(IsTrue(Points.Last().Equals(ATennisCourt::LocalMetersToWorld(Court, FVector(0.9, 0.0, 1.0)), 1e-9)));
	}

	// 最後の状態が間引きの位置にちょうど来るときは、重ねて入れない
	TEST_METHOD(Polyline_DoesNotDuplicateLastState)
	{
		ASSERT_THAT(AreEqual(3, TennisBallDebugDraw::TrajectoryPolyline(StraightTrajectory(9), FTransform::Identity, 4).Num()));
	}

	// 堅牢性：空の予測は空、Stride が 1 未満なら 1（全部の状態）
	TEST_METHOD(Polyline_HandlesEmptyAndInvalidStride)
	{
		ASSERT_THAT(AreEqual(0, TennisBallDebugDraw::TrajectoryPolyline(FTrajectory(), FTransform::Identity, 4).Num()));
		ASSERT_THAT(AreEqual(5, TennisBallDebugDraw::TrajectoryPolyline(StraightTrajectory(5), FTransform::Identity, 0).Num()));
	}
};
