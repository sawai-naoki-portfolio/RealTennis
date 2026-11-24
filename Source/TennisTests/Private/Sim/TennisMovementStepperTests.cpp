// W-3.5、T-MOVE-005：移動を固定の刻み（240 Hz）で進める（DD-03 §2.2・§2.4）。外（CMC）は返した速度でフレームの時間だけ動く。
// フレームの分け方（240 で割り切れないものを含む）によらず、外の位置は「範囲に収めながらモデルを同じ数だけステップした位置
// + 今の速度 × 余りの時間（範囲に収める）」と合い、速さは最高速度 × ダッシュの倍率を超えない（範囲の端を斜めに押し続けても）。
#include "CQTest.h"
#include "FixedStepClock.h"
#include "Movement/TennisMovement.h"
#include "TennisConstants.h"

#include <limits>

namespace
{
	struct FScenario
	{
		FVector2D Start;
		FVector2D Input;
	};

	struct FRunResult
	{
		FVector2D Position;      // 外の位置
		FVector2D Expected;      // 参照：モデルを 1 ステップずつ進めて範囲に収めた位置 + 余りの分
		FVector2D ModelVelocity;      // モデルの速度（打点の探索が読む）
		FVector2D ReferenceVelocity;  // 参照の速度
		double MaxSpeed = 0.0;   // 外が動いた速さの最大（最初のフレームを除く）
	};

	FRunResult Run(const FScenario& Scenario, const TArray<double>& Frames, double Seconds, double TimeScale = 1.0)
	{
		const FMovementParams Params;
		FMovementStepper Stepper;
		FRunResult R;
		R.Position = Scenario.Start;

		FFixedStepClock ReferenceClock;  // 同じ時間から同じ数のステップを作る
		FMovementState ReferenceState;
		FVector2D Reference = Scenario.Start;
		double Sum = 0.0;
		for (const double Frame : Frames)
		{
			Sum += Frame;
		}
		const int32 Count = FMath::RoundToInt32(Seconds * Frames.Num() / Sum);  // フレームの数で決める（時間の足し算の端数に左右されない）
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double Frame = Frames[Index % Frames.Num()];
			const FVector2D Velocity = TennisMovement::AdvanceFrame(Stepper, R.Position, Scenario.Input, -1.0, FMovementModifiers(), Params, Frame, TimeScale);
			R.Position += Velocity * Frame;  // CMC と同じ：返した速度でフレームの時間だけ動く
			if (Index > 0)
			{
				R.MaxSpeed = FMath::Max(R.MaxSpeed, Velocity.Size());
			}

			const int32 Steps = ReferenceClock.Advance(Frame, TimeScale);
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				ReferenceState = TennisMovement::Step(ReferenceState, Scenario.Input, -1.0, FMovementModifiers(), Params, TennisConst::SimDt);
				Reference += ReferenceState.Velocity * TennisConst::SimDt;
				TennisMovement::ClampToArea(Reference, ReferenceState.Velocity, -1.0);
			}
			R.Expected = Reference + ReferenceState.Velocity * ReferenceClock.Remainder();
			FVector2D Unused = ReferenceState.Velocity;
			TennisMovement::ClampToArea(R.Expected, Unused, -1.0);
		}
		R.ModelVelocity = Stepper.State.Velocity;
		R.ReferenceVelocity = ReferenceState.Velocity;
		return R;
	}

	const TArray<TArray<double>> Partitions = {
		{ 1.0 / 30.0 }, { 1.0 / 60.0 }, { 1.0 / 144.0 }, { 1.0 / 500.0 }, { 0.016, 0.007, 0.033, 0.001, 0.02 } };
	const double TopSpeed = FMovementParams().MaxSpeed * FMovementParams().DashSpeedFactor + 1e-9;
}

TEST_CLASS(MovementStepper, "Tennis.Sim.Movement")
{
	// 広いところ・ネットの手前を斜めに押し続ける・サイドの端に沿う：どの分け方でも参照と合い、速さは上限を超えない
	TEST_METHOD(AdvanceFrame_FollowsTheClampedFixedStepModelForAnyFramePartition)
	{
		const FScenario Scenarios[] = {
			{ FVector2D(-8.0, 0.0), FVector2D(0.0, 1.0) },    // 広いところ
			{ FVector2D(-0.6, 0.0), FVector2D(0.6, 0.8) },    // ネットの手前（|X| ≥ 0.45）を斜めに押す
			{ FVector2D(-5.0, 8.5), FVector2D(0.6, 0.8) },    // サイドの端（|Y| ≤ 8.985）に沿う
		};
		for (const FScenario& Scenario : Scenarios)
		for (const TArray<double>& Frames : Partitions)
		{
			const FRunResult R = Run(Scenario, Frames, 2.0);
			const FString Case = FString::Printf(TEXT("start %s, frame %.4f"), *Scenario.Start.ToString(), Frames[0]);
			ASSERT_THAT(IsTrue(R.Position.Equals(R.Expected, 1e-9), *FString::Printf(TEXT("%s: %s vs %s"), *Case, *R.Position.ToString(), *R.Expected.ToString())));
			ASSERT_THAT(IsTrue(R.MaxSpeed <= TopSpeed, *FString::Printf(TEXT("%s: speed %.3f"), *Case, R.MaxSpeed)));
			ASSERT_THAT(IsTrue(R.ModelVelocity.Equals(R.ReferenceVelocity, 1e-12)));  // 打点の探索が読むモデルの速度も、分け方によらない
		}
	}

	// 同じ数のステップのモデルの位置は、分け方によらず同じ（入力が一定なら、どの分け方でも同じステップを踏む）
	TEST_METHOD(AdvanceFrame_SameStepsGiveTheSamePositionAcrossPartitions)
	{
		const FScenario Scenario{ FVector2D(-0.6, 0.0), FVector2D(0.6, 0.8) };
		const FRunResult A = Run(Scenario, { 1.0 / 60.0 }, 1.5);     // 90 フレーム ≒ 360 ステップ
		const FRunResult B = Run(Scenario, { 1.0 / 120.0 }, 1.5);    // 180 フレーム ≒ 360 ステップ（端数で 1 ステップずれても、余りの分で合う）
		ASSERT_THAT(IsTrue(A.Expected.Equals(B.Expected, 1e-3)));      // 1 mm まで（1 ステップずれたときの、ステップの中の加速の分は 0.1 mm 未満）
		ASSERT_THAT(IsTrue(A.Position.Equals(B.Position, 1e-3)));
	}

	// 範囲の外から始めても、範囲の中に入れて続ける（最初のフレームで戻す）
	TEST_METHOD(AdvanceFrame_BringsAPositionOutsideTheAreaBack)
	{
		const FRunResult R = Run({ FVector2D(-1.0, 9.5), FVector2D::ZeroVector }, { 1.0 / 60.0 }, 0.5);
		ASSERT_THAT(IsNear(8.985, R.Position.Y, 1e-6));
		ASSERT_THAT(IsTrue(R.Position.Equals(R.Expected, 1e-9)));
	}

	// 時間の倍率（tennis.TimeScale）：0.5 倍で 2 s 動くと、1 倍で 1 s 動いたのと同じ位置
	TEST_METHOD(AdvanceFrame_FollowsTheTimeScale)
	{
		const FScenario Scenario{ FVector2D(-8.0, 0.0), FVector2D(0.0, 1.0) };
		const FRunResult Half = Run(Scenario, { 1.0 / 60.0 }, 2.0, 0.5);
		const FRunResult Normal = Run(Scenario, { 1.0 / 60.0 }, 1.0);
		ASSERT_THAT(IsTrue(Half.Position.Equals(Normal.Position, 1e-6)));
	}

	// 外が動けなかったとき（衝突）は、追いつこうとせずに、外の位置から続ける
	TEST_METHOD(AdvanceFrame_ContinuesFromWhereTheOutsideStopped)
	{
		const FMovementParams Params;
		FMovementStepper Stepper;
		FVector2D Position(-8.0, 0.0);
		for (int32 Frame = 0; Frame < 120; ++Frame)  // 2 s で最高速度
		{
			Position += TennisMovement::AdvanceFrame(Stepper, Position, FVector2D(0.0, 1.0), -1.0, FMovementModifiers(), Params, 1.0 / 60.0) / 60.0;
		}
		// 外は 1 フレームの間、動けなかった（Position をそのまま）
		TennisMovement::AdvanceFrame(Stepper, Position, FVector2D(0.0, 1.0), -1.0, FMovementModifiers(), Params, 1.0 / 60.0);
		const FVector2D Velocity = TennisMovement::AdvanceFrame(Stepper, Position, FVector2D(0.0, 1.0), -1.0, FMovementModifiers(), Params, 1.0 / 60.0);
		ASSERT_THAT(IsTrue(Velocity.Size() <= TopSpeed, *Velocity.ToString()));
	}

	// 0・負・NaN のフレームは進めない
	TEST_METHOD(AdvanceFrame_IgnoresNonPositiveFrames)
	{
		FMovementStepper Stepper;
		const FVector2D Position(-8.0, 0.0);
		TennisMovement::AdvanceFrame(Stepper, Position, FVector2D(0.0, 1.0), -1.0, FMovementModifiers(), FMovementParams(), 1.0 / 60.0);
		const FMovementStepper Before = Stepper;
		for (const double Frame : { 0.0, -0.01, std::numeric_limits<double>::quiet_NaN() })
		{
			const FVector2D Velocity = TennisMovement::AdvanceFrame(Stepper, Position, FVector2D(0.0, 1.0), -1.0, FMovementModifiers(), FMovementParams(), Frame);
			ASSERT_THAT(IsTrue(Velocity.Equals(Before.State.Velocity, 0.0)));
		}
		ASSERT_THAT(IsTrue(Stepper.Lead.Equals(Before.Lead, 0.0) && Stepper.Clock.Remainder() == Before.Clock.Remainder()));
	}
};
