// W-3.5、T-MOVE-006：自動移動（DD-03 §2.3）。打点に入る位置へ、到着の時刻の SettleTime 前に止まれるように向かい、
// 余裕があれば急がない。間に合わなければ最高速度で向かう。入力はステップごとに決めるので、フレームの分け方によらない。
#include "CQTest.h"
#include "Movement/TennisMovement.h"
#include "Reach/TennisReach.h"
#include "TennisConstants.h"

#include <limits>

namespace
{
	const double Settle = FReachParams().FullMargin;  // 止まっていたい時間（DD-03 §2.3。0.15 s）

	struct FAutoRun
	{
		FVector2D Position;
		double ArriveTime = -1.0;  // 目標から 3 cm 以内で、速さが 0.2 m/s 未満になった最初の時刻（s）
		double PeakSpeed = 0.0;
		double FinalSpeed = 0.0;
	};

	// 外（CMC）と同じく、返した速度でフレームの時間だけ動かす。TimeLeft + 0.5 s まで
	FAutoRun Run(const FVector2D& Start, const FVector2D& Target, double TimeLeft, double Frame = 1.0 / 60.0,
		const FVector2D& StartVelocity = FVector2D::ZeroVector)
	{
		FMovementStepper Stepper;
		Stepper.State.Velocity = StartVelocity;
		Stepper.AutoMove = FAutoMoveTarget{ Target, TimeLeft, Settle };
		FAutoRun R;
		R.Position = Start;
		const int32 Frames = FMath::CeilToInt32((TimeLeft + 0.5) / Frame);
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			const FVector2D Velocity = TennisMovement::AdvanceFrame(Stepper, R.Position, FVector2D(1.0, 0.0), -1.0, FMovementModifiers(), FMovementParams(), Frame);
			R.Position += Velocity * Frame;
			R.PeakSpeed = FMath::Max(R.PeakSpeed, Stepper.State.Velocity.Size());
			if (R.ArriveTime < 0.0 && FVector2D::Distance(R.Position, Target) <= 0.03 && Stepper.State.Velocity.Size() < 0.2)
			{
				R.ArriveTime = (Index + 1) * Frame;
			}
		}
		R.FinalSpeed = Stepper.State.Velocity.Size();
		return R;
	}
}

TEST_CLASS(AutoMove, "Tennis.Sim.Movement")
{
	// 間に合う距離なら、到着の時刻の SettleTime 前までに目標で止まる（横・後ろ・近く）。スティックの入力（+X）は使わない
	TEST_METHOD(AutoMove_SettlesAtTheTargetBeforeTheHit)
	{
		struct FCase { FVector2D Start; FVector2D Target; double TimeLeft; };
		const FCase Cases[] = {
			{ FVector2D(-10.0, 0.0), FVector2D(-10.0, 3.0), 1.5 },   // 横 3 m
			{ FVector2D(-10.0, 0.0), FVector2D(-10.0, -3.0), 3.0 },  // 横 3 m、余裕あり
			{ FVector2D(-8.0, 0.0), FVector2D(-10.0, 0.0), 1.5 },    // 後ろ 2 m（後退の最高速度と加速）
			{ FVector2D(-10.0, 0.0), FVector2D(-10.0, 0.5), 1.0 },   // 近く
		};
		for (const FCase& Case : Cases)
		{
			const FAutoRun R = Run(Case.Start, Case.Target, Case.TimeLeft);
			const FString Name = FString::Printf(TEXT("to %s in %.1f s: arrive %.3f s"), *Case.Target.ToString(), Case.TimeLeft, R.ArriveTime);
			ASSERT_THAT(IsTrue(R.ArriveTime > 0.0 && R.ArriveTime <= Case.TimeLeft - Settle, *Name));
			ASSERT_THAT(IsTrue(FVector2D::Distance(R.Position, Case.Target) <= 0.03 && R.FinalSpeed < 1e-9, *Name));
		}
	}

	// 余裕があれば急がない（3 m を 3 s：最高速度 5.5 m/s の半分も出さない）
	TEST_METHOD(AutoMove_DoesNotSprintWithTimeToSpare)
	{
		const FAutoRun R = Run(FVector2D(-10.0, 0.0), FVector2D(-10.0, 3.0), 3.0);
		ASSERT_THAT(IsTrue(R.PeakSpeed < 2.0, *FString::Printf(TEXT("peak %.3f m/s"), R.PeakSpeed)));
	}

	// 間に合わなければ、最高速度で向かう（6 m を 1 s）
	TEST_METHOD(AutoMove_RunsAtTopSpeedWhenLate)
	{
		const FAutoRun R = Run(FVector2D(-10.0, 0.0), FVector2D(-10.0, 6.0), 1.0);
		ASSERT_THAT(IsNear(FMovementParams().MaxSpeed, R.PeakSpeed, 1e-9));
	}

	// 入力はステップごとに決めるので、フレームの分け方によらず同じ位置で止まる
	TEST_METHOD(AutoMove_StopsAtTheSamePlaceForAnyFramePartition)
	{
		const FAutoRun A = Run(FVector2D(-10.0, 0.0), FVector2D(-9.0, 2.5), 1.4, 1.0 / 60.0, FVector2D(0.0, -3.0));
		const FAutoRun B = Run(FVector2D(-10.0, 0.0), FVector2D(-9.0, 2.5), 1.4, 1.0 / 144.0, FVector2D(0.0, -3.0));
		ASSERT_THAT(IsTrue(A.Position.Equals(B.Position, 1e-9), *FString::Printf(TEXT("%s vs %s"), *A.Position.ToString(), *B.Position.ToString())));
	}

	// 後退で間に合わないときも、入力の長さは 1（後退の最高速度で向かう）。逆へ動いている状態からでも、間に合えば止まる
	TEST_METHOD(AutoMove_BackpedalsAtFullInputAndRecoversFromMovingAway)
	{
		const FVector2D Late = TennisMovement::AutoMoveInput(FVector2D(-8.0, 0.0), FAutoMoveTarget{ FVector2D(-12.0, 0.0), 0.3, Settle }, -1.0, FMovementParams());
		ASSERT_THAT(IsTrue(Late.Equals(FVector2D(-1.0, 0.0), 1e-12)));

		const FAutoRun R = Run(FVector2D(-10.0, 0.0), FVector2D(-10.0, 2.0), 2.0, 1.0 / 60.0, FVector2D(0.0, -4.0));  // 4 m/s で逆へ
		ASSERT_THAT(IsTrue(R.ArriveTime > 0.0 && R.ArriveTime <= 2.0 - Settle, *FString::Printf(TEXT("arrive %.3f s"), R.ArriveTime)));
	}

	// 着いていれば・目標が数でなければ、入力は 0
	TEST_METHOD(AutoMoveInput_IsZeroAtTheTargetOrForAnInvalidTarget)
	{
		const FMovementParams Params;
		ASSERT_THAT(IsTrue(TennisMovement::AutoMoveInput(FVector2D(-10.0, 0.01), FAutoMoveTarget{ FVector2D(-10.0, 0.0), 1.0, Settle }, -1.0, Params).IsZero()));
		const double NaN = std::numeric_limits<double>::quiet_NaN();
		ASSERT_THAT(IsTrue(TennisMovement::AutoMoveInput(FVector2D(-10.0, 0.0), FAutoMoveTarget{ FVector2D(NaN, 0.0), 1.0, Settle }, -1.0, Params).IsZero()));
		const FVector2D Late = TennisMovement::AutoMoveInput(FVector2D(-10.0, 0.0), FAutoMoveTarget{ FVector2D(-10.0, 4.0), -0.2, Settle }, -1.0, Params);
		ASSERT_THAT(IsTrue(Late.Equals(FVector2D(0.0, 1.0), 1e-12)));  // 時刻を過ぎても、最高速度で向かう
	}
};
