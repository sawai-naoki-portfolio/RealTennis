// W-3.6、T-MOVE-007：軌跡の予測 TennisMovement::PredictPath（DD-03 §6.5）。今のモデルの状態から、同じ入力（自動移動の間はステップごとの
// 自動移動の入力）・同じ修飾で Step を固定の刻みで進めた位置と、指定の時刻で同じ（刻みの間は前後のステップの補間）。範囲に収める。
// 数でない・負・上限を超える時刻。
#include "CQTest.h"
#include "Movement/TennisMovement.h"
#include "TennisConstants.h"

#include <limits>

namespace
{
	constexpr double PathTolerance = 1e-9;  // m（T-MOVE-007）
	constexpr double NearSide = -1.0;   // 手前の選手（ネットは +X）

	/** 参照：公開の Step・AutoMoveInput・ClampToArea で 1 ステップずつ進めた、ステップごとの位置（0 番は今） */
	TArray<FVector2D> ReferencePath(FMovementState State, FVector2D Position, TOptional<FAutoMoveTarget> AutoMove, FVector2D Input,
		const FMovementModifiers& Modifiers, const FMovementParams& Params, int32 Steps)
	{
		TArray<FVector2D> Path = { Position };
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			const FVector2D StepInput = AutoMove.IsSet() ? TennisMovement::AutoMoveInput(Position, *AutoMove, NearSide, Params) : Input;
			State = TennisMovement::Step(State, StepInput, NearSide, Modifiers, Params, TennisConst::SimDt);
			Position += State.Velocity * TennisConst::SimDt;
			TennisMovement::ClampToArea(Position, State.Velocity, NearSide);
			if (AutoMove.IsSet())
			{
				AutoMove->TimeLeft -= TennisConst::SimDt;
			}
			Path.Add(Position);
		}
		return Path;
	}

	int32 StepsAt(double Time)
	{
		return FMath::RoundToInt32(Time / TennisConst::SimDt);
	}
}

TEST_CLASS(MovementPredict, "Tennis.Sim.Movement")
{
	const FMovementParams Params;

	// 手の入力：刻みの上の時刻はそのステップの位置、間の時刻は前後のステップの補間。Times の順で返す
	TEST_METHOD(PredictPath_MatchesSteppingWithTheSameInput)
	{
		FMovementState State;
		State.Velocity = FVector2D(1.0, 0.5);
		const FVector2D Start(-10.0, 1.0);
		const FVector2D Input(0.3, 0.9);
		const double Times[] = { 0.8, 0.0, 0.1, 0.2, 0.4, 0.1021 };
		const TArray<FVector2D> Path = TennisMovement::PredictPath(State, Start, {}, Input, NearSide, {}, Params, Times);
		const TArray<FVector2D> Ref = ReferencePath(State, Start, {}, Input, {}, Params, StepsAt(0.8));
		ASSERT_THAT(AreEqual(6, Path.Num()));
		for (int32 Index = 0; Index < 5; ++Index)
		{
			ASSERT_THAT(IsTrue(Path[Index].Equals(Ref[StepsAt(Times[Index])], PathTolerance), *FString::Printf(TEXT("t=%.4f"), Times[Index])));
		}
		const double At = 0.1021 / TennisConst::SimDt;  // 24.504 ステップ
		ASSERT_THAT(IsTrue(Path[5].Equals(FMath::Lerp(Ref[24], Ref[25], At - 24.0), PathTolerance)));
		ASSERT_THAT(IsTrue(Path[1].Equals(Start, 0.0)));  // 時刻 0 は今の位置
	}

	// 自動移動：入力の代わりにステップごとの自動移動の入力で進み、目標へ近づく
	TEST_METHOD(PredictPath_FollowsTheAutoMoveTarget)
	{
		FAutoMoveTarget AutoMove;
		AutoMove.Target = FVector2D(-9.0, 3.0);
		AutoMove.TimeLeft = 0.9;
		AutoMove.SettleTime = 0.2;
		const FVector2D Start(-10.0, 0.0);
		const double Times[] = { 0.2, 0.4, 0.8 };
		const TArray<FVector2D> Path = TennisMovement::PredictPath({}, Start, AutoMove, FVector2D(-1.0, 0.0), NearSide, {}, Params, Times);
		const TArray<FVector2D> Ref = ReferencePath({}, Start, AutoMove, FVector2D(-1.0, 0.0), {}, Params, StepsAt(0.8));
		ASSERT_THAT(AreEqual(3, Path.Num()));
		for (int32 Index = 0; Index < 3; ++Index)
		{
			ASSERT_THAT(IsTrue(Path[Index].Equals(Ref[StepsAt(Times[Index])], PathTolerance)));
		}
		ASSERT_THAT(IsTrue(FVector2D::Distance(Path[2], AutoMove.Target) < FVector2D::Distance(Start, AutoMove.Target) - 2.0));
	}

	// 範囲の端：ネットへ押し続けても、ネットの手前に収める（AdvanceFrame と同じ ClampToArea）
	TEST_METHOD(PredictPath_StaysInTheArea)
	{
		FMovementState State;
		State.Velocity = FVector2D(Params.MaxSpeed, 0.0);
		const FVector2D Start(-0.6, 0.0);
		const double Times[] = { 0.5 };
		const TArray<FVector2D> Path = TennisMovement::PredictPath(State, Start, {}, FVector2D(1.0, 0.0), NearSide, {}, Params, Times);
		const TArray<FVector2D> Ref = ReferencePath(State, Start, {}, FVector2D(1.0, 0.0), {}, Params, StepsAt(0.5));
		ASSERT_THAT(AreEqual(1, Path.Num()));
		ASSERT_THAT(IsTrue(Path[0].Equals(Ref.Last(), PathTolerance)));
		ASSERT_THAT(IsTrue(Path[0].X <= -(TennisMovement::NetGap + TennisMovement::BodyRadius) + 1e-12));
	}

	// 修飾（フォロースルーの速さの制限）を、すべてのステップに使う
	TEST_METHOD(PredictPath_UsesTheModifiers)
	{
		FMovementModifiers FollowThrough;
		FollowThrough.bFollowThrough = true;
		const FVector2D Start(-10.0, 0.0);
		const double Times[] = { 0.8 };
		const TArray<FVector2D> Slow = TennisMovement::PredictPath({}, Start, {}, FVector2D(0.0, 1.0), NearSide, FollowThrough, Params, Times);
		const TArray<FVector2D> Free = TennisMovement::PredictPath({}, Start, {}, FVector2D(0.0, 1.0), NearSide, {}, Params, Times);
		const TArray<FVector2D> Ref = ReferencePath({}, Start, {}, FVector2D(0.0, 1.0), FollowThrough, Params, StepsAt(0.8));
		ASSERT_THAT(AreEqual(1, Slow.Num()));
		ASSERT_THAT(AreEqual(1, Free.Num()));
		ASSERT_THAT(IsTrue(Slow[0].Equals(Ref.Last(), PathTolerance)));
		ASSERT_THAT(IsTrue(Slow[0].Y - Start.Y < Free[0].Y - Start.Y));
	}

	// 数でない・負の時刻は 0（今の位置）、上限を超える時刻は MaxPredictTime。時刻がなければ空
	TEST_METHOD(PredictPath_ClampsBadTimes)
	{
		const FVector2D Start(-10.0, 0.0);
		const double Times[] = { std::numeric_limits<double>::quiet_NaN(), -1.0, 5.0, std::numeric_limits<double>::infinity() };
		const TArray<FVector2D> Path = TennisMovement::PredictPath({}, Start, {}, FVector2D(0.0, 1.0), NearSide, {}, Params, Times);
		const TArray<FVector2D> Ref = ReferencePath({}, Start, {}, FVector2D(0.0, 1.0), {}, Params, StepsAt(TennisMovement::MaxPredictTime));
		ASSERT_THAT(AreEqual(4, Path.Num()));
		ASSERT_THAT(IsTrue(Path[0].Equals(Start, 0.0) && Path[1].Equals(Start, 0.0) && Path[3].Equals(Start, 0.0)));  // 無限大も数でない時刻
		ASSERT_THAT(IsTrue(Path[2].Equals(Ref.Last(), PathTolerance)));
		ASSERT_THAT(AreEqual(0, TennisMovement::PredictPath({}, Start, {}, FVector2D(0.0, 1.0), NearSide, {}, Params, {}).Num()));
	}
};
