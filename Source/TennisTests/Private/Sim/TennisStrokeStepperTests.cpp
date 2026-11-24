// W-3.5、T-STRK-014：ストロークの進行（DD-03 §6.4・§6.6）。W-3.6、T-STRK-018：下半身を Motion Matching と混ぜる重み（§6.5）。固定の刻みでモンタージュの位置・再生速度・体の向きを進め、
// インパクトの正確な時刻と終わり（RecoverStart）を返す。フレームの分け方によらない。
#include "CQTest.h"
#include "Stroke/TennisStrokeStepper.h"
#include "TennisConstants.h"

#include <limits>

namespace
{
	constexpr double Dt = TennisConst::SimDt;
	const FStrokeClip Clip{ 23.0 / 30.0, 38.0 / 30.0 };  // 仮のストローク：インパクト 0.767 s、RecoverStart 1.267 s
	const double TargetYaw = FMath::DegreesToRadians(-20.0);

	struct FRun
	{
		FStrokeStepper Stepper;
		double ImpactAt = -1.0;  // 始めてからインパクトまで（s。ステップの端数まで）
		double EndedAt = -1.0;   // 始めてから終わりまで（s。フレームの終わりの時刻）
		int32 Impacts = 0;
		double Elapsed = 0.0;
	};

	/** 体の向き（rad）：始め・インパクト・ネットの方向 */
	struct FYaws
	{
		double Start = 0.0;
		double Target = TargetYaw;
		double Net = 0.0;
	};

	/** 角度の差（近い回り） */
	double AngleGap(double A, double B)
	{
		return FMath::Abs(TennisStrokeTiming::WrapAngle(A - B));
	}

	/**
	 * 打点の時刻（始めてから HitTime 秒後。ShiftAt 秒の時点で Shift 秒ずらせる）へ向けて振る。フレームは StepsPerFrame の列を繰り返す。
	 * FreezeAfter より後は、ボールの時計が止まったことにする（打点までの残りが変わらない）。
	 */
	FRun Run(double HitTime, const TArray<int32>& StepsPerFrame, double ShiftAt = -1.0, double Shift = 0.0, double FreezeAfter = -1.0,
		const FYaws& Yaws = FYaws())
	{
		const FStrokeSyncParams Params;
		FRun R;
		TennisStrokeStep::Begin(R.Stepper, Clip, HitTime, Yaws.Start, Yaws.Target, Params);
		double Frozen = 0.0;
		for (int32 Frame = 0; Frame < 2000 && R.Stepper.State != EStrokeState::None; ++Frame)
		{
			const int32 Steps = StepsPerFrame[Frame % StepsPerFrame.Num()];
			const double End = R.Elapsed + Steps * Dt;
			if (ShiftAt >= 0.0 && R.Elapsed < ShiftAt && End >= ShiftAt)
			{
				HitTime += Shift;
			}
			double Available = HitTime - End;
			if (FreezeAfter >= 0.0)
			{
				Frozen = R.Elapsed < FreezeAfter ? Available : Frozen;
				Available = R.Elapsed < FreezeAfter ? Available : Frozen;
			}
			const FStrokeStepEvents Events = TennisStrokeStep::Advance(R.Stepper, Steps, Available, Yaws.Net, Params);
			R.Elapsed = End;
			if (Events.bImpact)
			{
				++R.Impacts;
				R.ImpactAt = End - Events.ImpactLead;
			}
			if (Events.bEnded)
			{
				R.EndedAt = End;
			}
		}
		return R;
	}
}

TEST_CLASS(StrokeStepper, "Tennis.Sim.Stroke")
{
	// 範囲の中の時間：先頭から振り、インパクトは打点の時刻にちょうど合う。その後 RecoverStart で終わり、状態は None に戻る
	TEST_METHOD(Advance_HitsOnTimeAndEndsAtRecoverStart)
	{
		const FRun R = Run(0.9, { 4 });  // 60 fps
		ASSERT_THAT(AreEqual(1, R.Impacts));
		ASSERT_THAT(IsNear(0.9, R.ImpactAt, 1e-9));
		const double Rate = Clip.ImpactTime / 0.9;
		ASSERT_THAT(IsTrue(R.EndedAt >= 0.9 + (Clip.RecoverTime - Clip.ImpactTime) / Rate && R.EndedAt <= 0.9 + (Clip.RecoverTime - Clip.ImpactTime) / Rate + 4 * Dt + 1e-9));
		ASSERT_THAT(IsTrue(R.Stepper.State == EStrokeState::None));
	}

	// フレームの分け方（60・240 fps、不ぞろい）によらず、インパクトの時刻と終わりの状態（位置・再生速度・向き）は同じ。
	// 範囲の中・待ってから振る（読み直しが効く）・途中から振る（補正が効く）のどれでも
	TEST_METHOD(Advance_ResultDoesNotDependOnTheFramePartition)
	{
		const FYaws Yaws{ FMath::DegreesToRadians(10.0), TargetYaw, FMath::DegreesToRadians(5.0) };
		for (const double HitTime : { 0.9, 2.0, 0.4 })
		{
			const FRun Base = Run(HitTime, { 4 }, -1.0, 0.0, -1.0, Yaws);
			for (const TArray<int32>& Partition : { TArray<int32>{ 1 }, TArray<int32>{ 3, 1, 7, 2, 5 } })
			{
				const FRun Other = Run(HitTime, Partition, -1.0, 0.0, -1.0, Yaws);
				const FString Label = FString::Printf(TEXT("hit %.1f s, %d steps first"), HitTime, Partition[0]);
				ASSERT_THAT(IsNear(Base.ImpactAt, Other.ImpactAt, 1e-9, *Label));
				ASSERT_THAT(IsNear(Base.Stepper.Position, Other.Stepper.Position, 1e-9, *Label));
				ASSERT_THAT(IsNear(Base.Stepper.PlayRate, Other.Stepper.PlayRate, 1e-9, *Label));
				ASSERT_THAT(IsNear(Base.Stepper.Yaw, Other.Stepper.Yaw, 1e-9, *Label));
			}
		}
	}

	// 読み直しは ReplanInterval（0.05 s）をステップ数に丸めた 12 ステップごと（時間を足し合わせると 13 ステップ目になる）
	TEST_METHOD(Advance_ReplansEveryTwelveSteps)
	{
		const FStrokeSyncParams Params;
		FStrokeStepper Stepper;
		ASSERT_THAT(IsTrue(TennisStrokeStep::Begin(Stepper, Clip, 2.0, 0.0, TargetYaw, Params)));
		const double Delay = Stepper.Delay;
		TennisStrokeStep::Advance(Stepper, 11, 2.0 - 11 * Dt - 0.5, 0.0, Params);  // 打点が 0.5 s 早くなった。11 ステップでは読み直さない
		ASSERT_THAT(IsNear(Delay - 11 * Dt, Stepper.Delay, 1e-12));
		TennisStrokeStep::Advance(Stepper, 1, 2.0 - 12 * Dt - 0.5, 0.0, Params);   // 12 ステップ目で読み直す
		ASSERT_THAT(IsNear(Delay - 0.5 - 12 * Dt, Stepper.Delay, 1e-9));
	}

	// 時間が余れば待ってから振り（Waiting）、足りなければ途中から振る。打点の時刻を過ぎていても、インパクトは 1 回だけ出る（すぐに）
	TEST_METHOD(Advance_WaitsSkipsOrStartsLate)
	{
		const FStrokeSyncParams Params;
		FStrokeStepper Waiting;
		ASSERT_THAT(IsTrue(TennisStrokeStep::Begin(Waiting, Clip, 2.0, 0.0, TargetYaw, Params)));
		ASSERT_THAT(IsTrue(Waiting.State == EStrokeState::Waiting && Waiting.Delay > 1.0));
		ASSERT_THAT(IsNear(2.0, Run(2.0, { 4 }).ImpactAt, 1e-9));

		FStrokeStepper Short;
		TennisStrokeStep::Begin(Short, Clip, 0.4, 0.0, TargetYaw, Params);
		ASSERT_THAT(IsTrue(Short.State == EStrokeState::Swinging && Short.Position > 0.2));
		ASSERT_THAT(IsNear(0.4, Run(0.4, { 4 }).ImpactAt, 1e-9));

		const FRun Late = Run(-0.1, { 4 });
		ASSERT_THAT(AreEqual(1, Late.Impacts));
		ASSERT_THAT(IsNear(0.0, Late.ImpactAt, 1e-9));
		ASSERT_THAT(IsTrue(Late.EndedAt > 0.0));
	}

	// 途中で打点の時刻が変わる：待っている間なら計画し直し、振っている間なら再生速度を補正して、インパクトを合わせる
	TEST_METHOD(Advance_FollowsAChangedHitTime)
	{
		const FRun WhileWaiting = Run(2.0, { 4 }, 0.3, -0.5);  // 0.3 s の時点で、打点が 0.5 s 早くなる
		ASSERT_THAT(IsTrue(FMath::Abs(WhileWaiting.ImpactAt - 1.5) <= Dt + 1e-9, *FString::Printf(TEXT("impact at %.4f"), WhileWaiting.ImpactAt)));
		for (const double Shift : { 0.03, -0.03 })
		{
			const FRun WhileSwinging = Run(0.9, { 4 }, 0.3, Shift);
			ASSERT_THAT(IsTrue(FMath::Abs(WhileSwinging.ImpactAt - (0.9 + Shift)) <= Dt + 1e-9, *FString::Printf(TEXT("shift %.2f: impact at %.4f"), Shift, WhileSwinging.ImpactAt)));
		}
	}

	// ボールの時計が止まっても（打点までの残りが変わらない）、ストロークはインパクトを出して終わりまで進む
	TEST_METHOD(Advance_FinishesEvenWhenTheBallClockStops)
	{
		const FRun R = Run(0.9, { 4 }, -1.0, 0.0, 0.3);
		ASSERT_THAT(AreEqual(1, R.Impacts));
		ASSERT_THAT(IsTrue(R.EndedAt > 0.9 && R.EndedAt < Clip.RecoverTime / FStrokeSyncParams().MinPlayRate + 0.1));
		ASSERT_THAT(IsTrue(R.Stepper.State == EStrokeState::None));
	}

	// 体の向き：始めの向きから、インパクトで計画の向きにちょうど着き、RecoverStart でネットの方向に戻る
	TEST_METHOD(Advance_TurnsToTheTargetYawAtImpactAndBackToTheNet)
	{
		const FStrokeSyncParams Params;
		FStrokeStepper Stepper;
		TennisStrokeStep::Begin(Stepper, Clip, 0.9, FMath::DegreesToRadians(10.0), TargetYaw, Params);
		ASSERT_THAT(IsNear(FMath::DegreesToRadians(10.0), Stepper.Yaw, 1e-12));
		double Elapsed = 0.0;
		double Previous = Stepper.Yaw;
		bool bImpact = false;
		while (!bImpact)
		{
			Elapsed += Dt;
			bImpact = TennisStrokeStep::Advance(Stepper, 1, 0.9 - Elapsed, 0.0, Params).bImpact;
			ASSERT_THAT(IsTrue(bImpact || Stepper.Yaw <= Previous + 1e-12));  // インパクトまでは、10° から −20° へ戻らずに回る
			Previous = Stepper.Yaw;
		}
		ASSERT_THAT(IsNear(TargetYaw, Stepper.Yaw, FMath::DegreesToRadians(0.2)));  // 越えたステップの分（1 ステップ）だけ戻り始めている
		const FRun R = Run(0.9, { 4 }, -1.0, 0.0, -1.0, FYaws{ FMath::DegreesToRadians(10.0), TargetYaw, FMath::DegreesToRadians(5.0) });
		ASSERT_THAT(IsNear(FMath::DegreesToRadians(5.0), R.Stepper.Yaw, 1e-9));  // 終わりはネットの方向（始めの向きではない）
	}

	// 奥の選手（ネットの方向が π）：±π をまたいで近い回りで回り、遠い回り（0 の側）を通らない
	TEST_METHOD(Advance_TurnsTheShortWayAcrossPi)
	{
		const FStrokeSyncParams Params;
		const double Start = UE_DOUBLE_PI - 0.2, Target = -UE_DOUBLE_PI + 0.3, Net = UE_DOUBLE_PI;  // 始め → 目標は +0.5 rad、目標 → ネットは −0.3 rad
		FStrokeStepper Stepper;
		TennisStrokeStep::Begin(Stepper, Clip, 0.9, Start, Target, Params);
		double Elapsed = 0.0;
		double Previous = Stepper.Yaw;
		bool bImpacted = false;
		bool bEnded = false;
		for (int32 Step = 0; Step < 1000 && !bEnded; ++Step)
		{
			Elapsed += Dt;
			const FStrokeStepEvents Events = TennisStrokeStep::Advance(Stepper, 1, 0.9 - Elapsed, Net, Params);
			if (Events.bImpact)
			{
				bImpacted = true;
				ASSERT_THAT(IsTrue(AngleGap(Target, Stepper.Yaw) < 0.01));
			}
			const double Turn = TennisStrokeTiming::WrapAngle(Stepper.Yaw - Previous);
			// インパクトまでは増え、その後は減る（近い回り）。インパクトのステップは、ステップの中で目標を越えて戻り始めるので、符号を決めない
			ASSERT_THAT(IsTrue(Events.bImpact || (bImpacted ? Turn <= 1e-12 : Turn >= -1e-12)));
			ASSERT_THAT(IsTrue(FMath::Abs(Stepper.Yaw) > 2.5));               // π の近くだけを通る
			Previous = Stepper.Yaw;
			bEnded = Events.bEnded;
		}
		ASSERT_THAT(IsTrue(bEnded && AngleGap(Net, Stepper.Yaw) < 1e-9));
	}

	// 待っている間は始めの向きのまま。途中から振っても（開始位置が正）、インパクトで計画の向きに着く
	TEST_METHOD(Advance_TurnsFromTheStartYawWhenWaitingOrSwingingFromTheMiddle)
	{
		const FStrokeSyncParams Params;
		const double Start = FMath::DegreesToRadians(10.0);
		FStrokeStepper Waiting;
		TennisStrokeStep::Begin(Waiting, Clip, 2.0, Start, TargetYaw, Params);
		TennisStrokeStep::Advance(Waiting, 24, 2.0 - 24 * Dt, 0.0, Params);
		ASSERT_THAT(IsTrue(Waiting.State == EStrokeState::Waiting && Waiting.Yaw == Waiting.StartYaw));

		FStrokeStepper Short;
		TennisStrokeStep::Begin(Short, Clip, 0.4, Start, TargetYaw, Params);
		ASSERT_THAT(IsTrue(Short.StartPosition > 0.2 && Short.Yaw == Short.StartYaw));
		// 開始位置からインパクトまでの間で 30° 回す：1 ステップは約 0.3°。インパクトのステップは向きを目標から計算し直すので、
		// Swinging の補間（開始位置からの進み具合）は、最初のステップとインパクトの直前のステップで確かめる
		const double OneStep = FMath::DegreesToRadians(0.5);
		double Elapsed = Dt;
		TennisStrokeStep::Advance(Short, 1, 0.4 - Elapsed, 0.0, Params);
		ASSERT_THAT(IsTrue(AngleGap(Start, Short.Yaw) < OneStep));
		double BeforeImpact = Short.Yaw;
		bool bImpact = false;
		for (int32 Step = 0; Step < 1000 && !bImpact; ++Step)
		{
			BeforeImpact = Short.Yaw;
			Elapsed += Dt;
			bImpact = TennisStrokeStep::Advance(Short, 1, 0.4 - Elapsed, 0.0, Params).bImpact;
		}
		ASSERT_THAT(IsTrue(bImpact && AngleGap(TargetYaw, BeforeImpact) < OneStep));
		ASSERT_THAT(IsTrue(AngleGap(TargetYaw, Short.Yaw) < OneStep));
	}

	// 始められない入力・振っていないときの Advance・取り消し
	TEST_METHOD(BeginAdvanceCancel_HandleInvalidAndIdleStates)
	{
		const FStrokeSyncParams Params;
		FStrokeStepper Stepper;
		const double NaN = std::numeric_limits<double>::quiet_NaN();
		ASSERT_THAT(IsFalse(TennisStrokeStep::Begin(Stepper, FStrokeClip{ 0.0, 1.0 }, 0.9, 0.0, 0.0, Params)));
		ASSERT_THAT(IsFalse(TennisStrokeStep::Begin(Stepper, FStrokeClip{ 0.8, 0.5 }, 0.9, 0.0, 0.0, Params)));  // RecoverStart がインパクトより前
		ASSERT_THAT(IsFalse(TennisStrokeStep::Begin(Stepper, Clip, NaN, 0.0, 0.0, Params)));
		ASSERT_THAT(IsTrue(Stepper.State == EStrokeState::None));
		const FStrokeStepEvents Idle = TennisStrokeStep::Advance(Stepper, 10, 0.5, 0.0, Params);
		ASSERT_THAT(IsFalse(Idle.bImpact || Idle.bEnded));

		ASSERT_THAT(IsFalse(TennisStrokeStep::Cancel(Stepper)));  // 振っていない

		ASSERT_THAT(IsTrue(TennisStrokeStep::Begin(Stepper, Clip, 0.9, 0.0, TargetYaw, Params)));
		ASSERT_THAT(IsFalse(TennisStrokeStep::Advance(Stepper, 0, 0.9, 0.0, Params).bImpact));
		TennisStrokeStep::Advance(Stepper, 40, 0.9 - 40 * Dt, 0.0, Params);
		const double Position = Stepper.Position;
		ASSERT_THAT(IsFalse(TennisStrokeStep::Begin(Stepper, Clip, NaN, 0.0, 0.0, Params)));  // 始められなければ、振っている途中のまま
		ASSERT_THAT(IsTrue(Stepper.State == EStrokeState::Swinging && Stepper.Position == Position));
		ASSERT_THAT(IsTrue(TennisStrokeStep::Cancel(Stepper)));  // インパクトの前は取り消せる
		ASSERT_THAT(IsTrue(Stepper.State == EStrokeState::None));
		const FStrokeStepEvents AfterCancel = TennisStrokeStep::Advance(Stepper, 400, 0.0, 0.0, Params);
		ASSERT_THAT(IsFalse(AfterCancel.bImpact || AfterCancel.bEnded));

		ASSERT_THAT(IsTrue(TennisStrokeStep::Begin(Stepper, Clip, 2.0, 0.0, TargetYaw, Params)));
		ASSERT_THAT(IsTrue(Stepper.State == EStrokeState::Waiting && TennisStrokeStep::Cancel(Stepper)));  // 待っている間も取り消せる
	}

	// インパクトの後に取り消しても、取り消さずに終わりまで進め、終わりを出す（DD-03 §6.6）
	TEST_METHOD(Cancel_AfterTheImpactStillReachesTheEnd)
	{
		const FStrokeSyncParams Params;
		FStrokeStepper Stepper;
		TennisStrokeStep::Begin(Stepper, Clip, 0.9, 0.0, TargetYaw, Params);
		double Elapsed = 0.0;
		bool bImpact = false;
		for (int32 Step = 0; Step < 1000 && !bImpact; ++Step)
		{
			Elapsed += Dt;
			bImpact = TennisStrokeStep::Advance(Stepper, 1, 0.9 - Elapsed, 0.0, Params).bImpact;
		}
		ASSERT_THAT(IsTrue(bImpact && Stepper.State == EStrokeState::FollowThrough));
		ASSERT_THAT(IsFalse(TennisStrokeStep::Cancel(Stepper)));
		ASSERT_THAT(IsTrue(Stepper.State == EStrokeState::FollowThrough));
		ASSERT_THAT(IsTrue(TennisStrokeStep::Advance(Stepper, 400, 0.9 - Elapsed - 400 * Dt, 0.0, Params).bEnded));
		ASSERT_THAT(IsTrue(Stepper.State == EStrokeState::None));
	}

	// 打点までの残りが数でなければ、読み直さずに計画のまま進める。ネットの方向が数でなければ、計画の向きのまま（数でない向きを出さない）
	TEST_METHOD(Advance_KeepsGoingWithInvalidAvailableOrNetYaw)
	{
		const FStrokeSyncParams Params;
		const double NaN = std::numeric_limits<double>::quiet_NaN();
		FStrokeStepper Stepper;
		TennisStrokeStep::Begin(Stepper, Clip, 0.9, 0.0, TargetYaw, Params);
		int32 Impacts = 0;
		double ImpactAt = -1.0;
		double Elapsed = 0.0;
		bool bEnded = false;
		for (int32 Frame = 0; Frame < 1000 && !bEnded; ++Frame)
		{
			const FStrokeStepEvents Events = TennisStrokeStep::Advance(Stepper, 4, NaN, NaN, Params);
			Elapsed += 4 * Dt;
			Impacts += Events.bImpact ? 1 : 0;
			ImpactAt = Events.bImpact ? Elapsed - Events.ImpactLead : ImpactAt;
			bEnded = Events.bEnded;
			ASSERT_THAT(IsTrue(FMath::IsFinite(Stepper.Yaw) && FMath::IsFinite(Stepper.PlayRate)));
		}
		ASSERT_THAT(IsTrue(bEnded));
		ASSERT_THAT(AreEqual(1, Impacts));
		ASSERT_THAT(IsNear(0.9, ImpactAt, 1e-9));  // 始めの計画のとおり
		ASSERT_THAT(IsNear(TargetYaw, Stepper.Yaw, 1e-12));
	}

	// T-STRK-018：振っていない・待つ・テイクバックは 1、スイングの手前の LowerBodyBlendTime で線形に 0、スイングから（インパクトを含む）は 0
	TEST_METHOD(LowerBodyLocomotionAlpha_IsOneUntilTheSwingAndZeroAtTheImpact)
	{
		const FStrokeSyncParams Params;  // SwingLead 0.30 s、LowerBodyBlendTime 0.10 s
		const double SwingStart = Clip.ImpactTime - Params.SwingLead;
		FStrokeStepper Stepper;
		Stepper.Clip = Clip;
		const auto AlphaAt = [&](EStrokeState State, double Position)
		{
			Stepper.State = State;
			Stepper.Position = Position;
			return TennisStrokeStep::LowerBodyLocomotionAlpha(Stepper, Params);
		};
		ASSERT_THAT(IsNear(1.0, AlphaAt(EStrokeState::None, 0.9), 1e-12));
		ASSERT_THAT(IsNear(1.0, AlphaAt(EStrokeState::Waiting, 0.0), 1e-12));
		ASSERT_THAT(IsNear(1.0, AlphaAt(EStrokeState::Swinging, 0.2), 1e-12));
		ASSERT_THAT(IsNear(1.0, AlphaAt(EStrokeState::Swinging, SwingStart - Params.LowerBodyBlendTime), 1e-12));
		ASSERT_THAT(IsNear(0.5, AlphaAt(EStrokeState::Swinging, SwingStart - 0.5 * Params.LowerBodyBlendTime), 1e-9));
		ASSERT_THAT(IsNear(0.0, AlphaAt(EStrokeState::Swinging, SwingStart), 1e-12));
		ASSERT_THAT(IsNear(0.0, AlphaAt(EStrokeState::Swinging, Clip.ImpactTime), 1e-12));  // インパクトでは必ず全身
		ASSERT_THAT(IsNear(0.0, AlphaAt(EStrokeState::FollowThrough, Clip.ImpactTime), 1e-12));
		ASSERT_THAT(IsNear(0.0, AlphaAt(EStrokeState::Swinging, std::numeric_limits<double>::quiet_NaN()), 1e-12));
	}

	// T-STRK-018：混ぜる時間が 0 ならスイングの始まりで切り替える。SwingLead がインパクトより長ければ、始めから全身
	TEST_METHOD(LowerBodyLocomotionAlpha_HandlesEdgeParams)
	{
		FStrokeSyncParams Params;
		Params.LowerBodyBlendTime = 0.0;
		FStrokeStepper Stepper;
		Stepper.Clip = Clip;
		Stepper.State = EStrokeState::Swinging;
		Stepper.Position = Clip.ImpactTime - Params.SwingLead - 1e-6;
		ASSERT_THAT(IsNear(1.0, TennisStrokeStep::LowerBodyLocomotionAlpha(Stepper, Params), 1e-12));
		Stepper.Position = Clip.ImpactTime - Params.SwingLead;
		ASSERT_THAT(IsNear(0.0, TennisStrokeStep::LowerBodyLocomotionAlpha(Stepper, Params), 1e-12));
		Params.SwingLead = 2.0;
		Stepper.Position = 0.0;
		ASSERT_THAT(IsNear(0.0, TennisStrokeStep::LowerBodyLocomotionAlpha(Stepper, Params), 1e-12));
	}
};
