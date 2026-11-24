// W-3.5、T-STRK-015：ストロークを振る（UTennisStrokeComponent。DD-03 §6.6）。ボールのステップの通し番号の差だけステッパを進め（§6.1 の時計）、
// インパクトの時刻 T がボールの時刻の上で打点の時刻に合う。止めたモンタージュの位置と体の向きはステッパに合う。インパクトから RecoverStart まで
// フォロースルーの制限。取り消しはインパクトの前だけ。取り消した後は体の向きをネットへ戻す（W-3.6、T-STRK-019。DD-03 §6.5）。
// ボールとストロークの Tick は、このテストが手で呼ぶ（ストロークはボールの後）。
#include "Animation/AnimInstance.h"
#include "Ball/TennisBall.h"
#include "Ball/TennisBallSimComponent.h"
#include "Components/ActorTestSpawner.h"
#include "Components/SkeletalMeshComponent.h"
#include "Court/TennisCourt.h"
#include "CQTest.h"
#include "Data/StrokeData.h"
#include "GameFramework/WorldSettings.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisMovementComponent.h"
#include "Player/TennisPlayerAppearance.h"
#include "Player/TennisReachComponent.h"
#include "Player/TennisStrokeComponent.h"
#include "Shot/TennisShotSolver.h"
#include "Misc/ScopeExit.h"
#include "TennisConstants.h"
#include "TennisGameTestHelpers.h"

#include <limits>

TEST_CLASS(TennisStrokeComponent, "Tennis.Game.Stroke.Component")
{
	FActorTestSpawner Spawner;
	ATennisBall* Ball = nullptr;
	ATennisCharacter* Character = nullptr;
	UTennisStrokeComponent* Stroke = nullptr;
	const TArray<float> Frames{ 1.0f / 60.0f, 1.0f / 144.0f, 1.0f / 30.0f, 1.0f / 90.0f };  // 不ぞろいのフレーム

	BEFORE_EACH()
	{
		Spawner.SpawnActor<ATennisCourt>();  // 原点（コート局所 = ワールド。1 m = 100 cm）
		Ball = &Spawner.SpawnActor<ATennisBall>();
		Character = &Spawner.SpawnActorAt<ATennisCharacter>(FVector(-1150.0, 100.0, 90.0), FRotator::ZeroRotator);
		Spawner.GetWorld().GetWorldSettings()->NotifyBeginPlay();  // コート・ボールを見つける（側・処理の順・探索の対象）
		Stroke = Character->GetStroke();
	}

	AFTER_EACH()
	{
		Spawner.GetWorld().EndPlay(EEndPlayReason::Quit);
	}

	/** 相手（+X の側）からのラリーの球を打ち出し、打点の候補を探す */
	bool LaunchAndFind(FHitCandidate& Candidate)
	{
		FShotRequest Request;
		Request.Origin = FVector(11.0, 0.5, 1.0);
		Request.Target = FVector(-9.0, -1.0, 0.0);
		Request.ConstraintValue = 0.9;
		Request.MinSpeed = 18.0;
		Request.MaxSpeed = 40.0;
		Request.Rpm = 2000.0;
		Request.Time = Ball->GetSim()->GetState().Time;  // 今打つ（前のテストの部分でボールの時刻が進んでいても）
		const FShotSolveResult Shot = FTennisShotSolver(Ball->GetSim()->GetContext()).Solve(Request);
		if (!Shot.bSolved)
		{
			return false;
		}
		Ball->GetSim()->Launch(Shot.Launch);
		return Character->GetReach()->FindBest(Candidate);
	}

	/** 1 フレーム：ボール → ストロークの順に Tick する */
	void TickFrame(float FrameTime)
	{
		Ball->GetSim()->TickComponent(FrameTime, LEVELTICK_All, nullptr);
		Stroke->TickComponent(FrameTime, LEVELTICK_All, nullptr);
	}

	struct FRecord
	{
		int32 Impacts = 0;
		int32 Finished = 0;
		double ImpactTime = -1.0;
		bool bFollowThroughAtImpact = false;
		bool bAutoMovingAtImpact = true;
		bool bFollowThroughAtFinish = true;
	};

	void Record(FRecord& R)
	{
		UTennisMovementComponent* Movement = Character->GetTennisMovement();
		Stroke->OnImpact.AddLambda([&R, Movement](const FStrokeImpact& Impact)
		{
			++R.Impacts;
			R.ImpactTime = Impact.Time;
			R.bFollowThroughAtImpact = Movement->GetModifiers().bFollowThrough;
			R.bAutoMovingAtImpact = Movement->IsAutoMoving();
		});
		Stroke->OnFinished.AddLambda([&R, Movement]()
		{
			++R.Finished;
			R.bFollowThroughAtFinish = Movement->GetModifiers().bFollowThrough;
		});
	}

	/** 止めたモンタージュの位置と体の向き（コート局所。コートは原点）が、ステッパと合う */
	bool BodyFollowsTheStepper() const
	{
		const UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance();
		const UAnimMontage* Montage = Stroke->GetStroke() != nullptr ? Stroke->GetStroke()->Montage.Get() : nullptr;
		const double Yaw = FMath::DegreesToRadians(Character->GetActorRotation().Yaw);
		return Anim != nullptr && Montage != nullptr && Anim->Montage_IsActive(Montage) && !Anim->Montage_IsPlaying(Montage)
			&& FMath::IsNearlyEqual(Anim->Montage_GetPosition(Montage), static_cast<float>(Stroke->GetStepper().Position), 1e-4f)
			&& FMath::Abs(TennisStrokeTiming::WrapAngle(Yaw - Stroke->GetStepper().Yaw)) < 1e-4;
	}

	/**
	 * 振り終わるまで進める。bBeginAfterBallTick なら、ボールが先にこのフレームの Tick を済ませた後で始める（候補が探索の Tick で見つかる場合）。
	 * 毎フレーム、打点までの残りが、進めたステップ数 × SimDt だけちょうど減ることも確かめる
	 */
	void RunStroke(bool bBeginAfterBallTick, FRecord& R)
	{
		FHitCandidate Candidate;
		ASSERT_THAT(IsTrue(LaunchAndFind(Candidate)));
		if (bBeginAfterBallTick)
		{
			Ball->GetSim()->TickComponent(Frames[0], LEVELTICK_All, nullptr);
		}
		ASSERT_THAT(IsTrue(Stroke->PlayStroke(Candidate, ETennisShotType::Topspin)));
		ASSERT_THAT(IsTrue(Stroke->IsStroking() && Character->GetTennisMovement()->IsAutoMoving()));
		ASSERT_THAT(IsTrue(BodyFollowsTheStepper()));
		Record(R);
		if (bBeginAfterBallTick)
		{
			Stroke->TickComponent(Frames[0], LEVELTICK_All, nullptr);  // このフレームの残り：ボールはもう進んでいる
		}

		const double Hit = Stroke->GetPlannedHit().Time;
		const UTennisBallSimComponent* Sim = Ball->GetSim();
		int64 Count = Sim->GetStepCount();
		double Remaining = Hit - Sim->GetState().Time;
		for (int32 Frame = 0; Frame < 2000 && R.Finished == 0; ++Frame)
		{
			TickFrame(Frames[Frame % Frames.Num()]);
			const double Now = Hit - Sim->GetState().Time;  // 前提の確認（T-BALL-024）：ステッパに渡す残りは、進めたステップ数 × SimDt だけ減る
			ASSERT_THAT(IsNear(Remaining - static_cast<double>(Sim->GetStepCount() - Count) * TennisConst::SimDt, Now, 1e-9));
			Count = Sim->GetStepCount();
			Remaining = Now;
			if (Stroke->IsStroking())
			{
				ASSERT_THAT(IsTrue(BodyFollowsTheStepper(), *FString::Printf(TEXT("frame %d"), Frame)));
			}
		}
	}

	// T-STRK-015：インパクトの時刻 T（ボールの時刻。ステップの端数まで）が、選び直した打点の時刻に合う。インパクトで自動移動を止めて
	// フォロースルーの制限を付け、RecoverStart で外して終わる。ストロークを始めたのがボールの Tick の前でも後でも同じ
	TEST_METHOD(PlayStroke_HitsAtThePlannedTimeWhenStartedBeforeTheBallTick)
	{
		FRecord R;
		RunStroke(false, R);
		ASSERT_THAT(AreEqual(1, R.Impacts));
		ASSERT_THAT(IsNear(Stroke->GetPlannedHit().Time, R.ImpactTime, 1e-9));
		ASSERT_THAT(IsTrue(R.bFollowThroughAtImpact && !R.bAutoMovingAtImpact));
		ASSERT_THAT(IsTrue(R.Finished == 1 && !R.bFollowThroughAtFinish && !Stroke->IsStroking()));
		const UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance();
		ASSERT_THAT(IsTrue(Anim->Montage_IsPlaying(Stroke->GetStroke()->Montage), TEXT("after recover start the montage plays to its end")));
	}

	TEST_METHOD(PlayStroke_HitsAtThePlannedTimeWhenStartedAfterTheBallTick)
	{
		FRecord R;
		RunStroke(true, R);
		ASSERT_THAT(AreEqual(1, R.Impacts));
		ASSERT_THAT(IsNear(Stroke->GetPlannedHit().Time, R.ImpactTime, 1e-9));
		ASSERT_THAT(IsTrue(R.Finished == 1 && !Stroke->IsStroking()));
	}

	// 取り消し（DD-03 §6.6）：インパクトの前なら止まり（知らせない）、インパクトの後は取り消さずに終わりまで進めて終わりを知らせる
	TEST_METHOD(CancelStroke_StopsOnlyBeforeTheImpact)
	{
		FRecord R;
		FHitCandidate Candidate;
		ASSERT_THAT(IsTrue(LaunchAndFind(Candidate) && Stroke->PlayStroke(Candidate, ETennisShotType::Topspin)));
		Record(R);
		TickFrame(Frames[0]);
		ASSERT_THAT(IsTrue(Stroke->CancelStroke()));
		ASSERT_THAT(IsFalse(Stroke->IsStroking()));
		ASSERT_THAT(IsTrue(Character->GetMesh()->GetAnimInstance()->Montage_GetIsStopped(Stroke->GetStroke()->Montage)));
		for (int32 Frame = 0; Frame < 200; ++Frame)
		{
			TickFrame(Frames[0]);
		}
		ASSERT_THAT(IsTrue(R.Impacts == 0 && R.Finished == 0));

		ASSERT_THAT(IsTrue(LaunchAndFind(Candidate) && Stroke->PlayStroke(Candidate, ETennisShotType::Topspin)));
		for (int32 Frame = 0; Frame < 2000 && R.Impacts == 0; ++Frame)
		{
			TickFrame(Frames[Frame % Frames.Num()]);
		}
		ASSERT_THAT(IsFalse(Stroke->CancelStroke()));  // インパクトの後
		ASSERT_THAT(IsTrue(Stroke->IsStroking() && Character->GetTennisMovement()->GetModifiers().bFollowThrough));
		for (int32 Frame = 0; Frame < 2000 && R.Finished == 0; ++Frame)
		{
			TickFrame(Frames[Frame % Frames.Num()]);
		}
		ASSERT_THAT(IsTrue(R.Finished == 1 && !Character->GetTennisMovement()->GetModifiers().bFollowThrough));
	}

	/** 振っている途中（bAfterImpact ならインパクトの後）にボールがなくなる */
	void LoseTheBall(bool bAfterImpact)
	{
		FRecord R;
		ON_SCOPE_EXIT
		{
			Stroke->OnImpact.Clear();  // R はこの関数の中だけ
			Stroke->OnFinished.Clear();
		};
		FHitCandidate Candidate;
		ASSERT_THAT(IsTrue(LaunchAndFind(Candidate) && Stroke->PlayStroke(Candidate, ETennisShotType::Topspin)));
		Record(R);
		for (int32 Frame = 0; Frame < 2000 && bAfterImpact && R.Impacts == 0; ++Frame)
		{
			TickFrame(Frames[Frame % Frames.Num()]);
		}
		UTennisMovementComponent* Movement = Character->GetTennisMovement();
		ASSERT_THAT(IsTrue(Stroke->IsStroking() && Movement->GetModifiers().bFollowThrough == bAfterImpact));
		Character->GetReach()->SetBall(nullptr);
		Stroke->TickComponent(Frames[0], LEVELTICK_All, nullptr);
		ASSERT_THAT(IsFalse(Stroke->IsStroking()));
		ASSERT_THAT(AreEqual(1, R.Finished));
		ASSERT_THAT(IsFalse(R.bFollowThroughAtFinish || Movement->GetModifiers().bFollowThrough));
		ASSERT_THAT(IsTrue(Character->GetMesh()->GetAnimInstance()->Montage_GetIsStopped(Stroke->GetStroke()->Montage)));
	}

	/**
	 * T-STRK-019（つなぎこみ）：取り消した後、体の向きをネットの方向（手前の選手は +X、ヨー 0）へ、1 フレームに 720°/s × 時間 × 時間の倍率まで
	 * 戻し、戻り切ったら Tick を止める。40° を 1/60 s のフレームで戻すのにかかるフレームの数が ExpectedFrames
	 */
	void TurnBackAfterCancel(float TimeScale, int32 ExpectedFrames)
	{
		const float SavedTimeScale = GetDefault<UTennisDebugSettings>()->GetTimeScale();
		TennisTest::SetTimeScale(TimeScale);
		ON_SCOPE_EXIT
		{
			TennisTest::SetTimeScale(SavedTimeScale);
		};
		FHitCandidate Candidate;
		ASSERT_THAT(IsTrue(LaunchAndFind(Candidate) && Stroke->PlayStroke(Candidate, ETennisShotType::Topspin)));
		TickFrame(Frames[0]);
		ASSERT_THAT(IsTrue(Stroke->CancelStroke()));
		Character->SetActorRotation(FRotator(0.0, 40.0, 0.0));  // ネットから 40° 振り向いたところで取り消した
		ASSERT_THAT(IsTrue(Stroke->IsComponentTickEnabled()));

		const double MaxStepDeg = Stroke->Sync.ReturnYawRateDeg / 60.0 * TimeScale;  // 倍率 1 で 12°
		double Previous = 40.0;
		int32 Frames60 = 0;
		for (; Frames60 < 40 && Stroke->IsComponentTickEnabled(); ++Frames60)
		{
			Stroke->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
			const double Yaw = Character->GetActorRotation().Yaw;
			ASSERT_THAT(IsTrue(Yaw <= Previous + 1e-6 && Previous - Yaw <= MaxStepDeg + 1e-4 && Yaw >= -1e-4,
				*FString::Printf(TEXT("frame %d: %.4f -> %.4f"), Frames60, Previous, Yaw)));
			Previous = Yaw;
		}
		ASSERT_THAT(IsNear(0.0, Previous, 1e-4));
		ASSERT_THAT(AreEqual(ExpectedFrames, Frames60));
		ASSERT_THAT(IsFalse(Stroke->IsComponentTickEnabled()));
	}

	TEST_METHOD(CancelStroke_TurnsTheBodyBackToTheNet)
	{
		TurnBackAfterCancel(1.0f, 4);  // 40° → 28 → 16 → 4 → 0、その Tick で止める
	}

	TEST_METHOD(CancelStroke_TurnsBackWithTheTimeScale)
	{
		TurnBackAfterCancel(0.5f, 7);  // 1 フレーム 6°：40° は 7 フレーム
	}

	// 戻す速さが 0 なら、戻さずに止める（Tick を続けない）
	TEST_METHOD(CancelStroke_StopsTurningWithZeroRate)
	{
		Stroke->Sync.ReturnYawRateDeg = 0.0;
		FHitCandidate Candidate;
		ASSERT_THAT(IsTrue(LaunchAndFind(Candidate) && Stroke->PlayStroke(Candidate, ETennisShotType::Topspin)));
		TickFrame(Frames[0]);
		ASSERT_THAT(IsTrue(Stroke->CancelStroke()));
		Character->SetActorRotation(FRotator(0.0, 40.0, 0.0));
		Stroke->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
		ASSERT_THAT(IsNear(40.0, Character->GetActorRotation().Yaw, 1e-4));
		ASSERT_THAT(IsFalse(Stroke->IsComponentTickEnabled()));
	}

	// 振っている途中にボールがなくなったら、警告を出して打ち切り、制限を外し、モンタージュを止め、終わりを 1 回知らせる（入力を待機に戻す）
	TEST_METHOD(LostBallBeforeTheImpact_StopsAndReportsTheEnd)
	{
		LoseTheBall(false);
	}

	TEST_METHOD(LostBallAfterTheImpact_StopsAndReportsTheEnd)
	{
		LoseTheBall(true);
	}

	// 始められない（数でない打点の時刻：選び直しと Begin が警告して止める）ときは、何もせずに false。振っている途中なら、前のものはそのまま。
	// Select・Begin の失敗の経路。Montage_Play の失敗の経路は、別の骨格のモンタージュが要るのでテストしない
	TEST_METHOD(PlayStroke_FailsWithoutChangingTheState)
	{
		FHitCandidate Candidate;
		ASSERT_THAT(IsTrue(LaunchAndFind(Candidate)));
		FHitCandidate Bad = Candidate;
		Bad.Time = std::numeric_limits<double>::quiet_NaN();
		ASSERT_THAT(IsFalse(Stroke->PlayStroke(Bad, ETennisShotType::Topspin)));
		ASSERT_THAT(IsFalse(Stroke->IsStroking() || Character->GetTennisMovement()->IsAutoMoving()));

		ASSERT_THAT(IsTrue(Stroke->PlayStroke(Candidate, ETennisShotType::Topspin)));
		const FStrokeStepper Before = Stroke->GetStepper();
		const double HitBefore = Stroke->GetPlannedHit().Time;
		ASSERT_THAT(IsFalse(Stroke->PlayStroke(Bad, ETennisShotType::Topspin)));
		ASSERT_THAT(IsTrue(Stroke->GetStepper().State == Before.State && Stroke->GetStepper().Position == Before.Position
			&& Stroke->GetPlannedHit().Time == HitBefore && BodyFollowsTheStepper()));
	}

	// 選手の見た目のデータに、選んだストロークの焼いた値がなければ始めない（ほかの体の値でずれを隠さない：DD-03 §6.6・§7）
	TEST_METHOD(PlayStroke_NeedsTheBakedSweetSpotOfTheAppearance)
	{
		const UTennisPlayerAppearance* Baked = Character->GetAppearance();
		ASSERT_THAT(IsTrue(Baked != nullptr && Baked->StrokeFits.Num() > 0, TEXT("run bake_player_appearances")));
		UTennisPlayerAppearance* NotBaked = NewObject<UTennisPlayerAppearance>();
		NotBaked->Body = Baked->Body;
		ASSERT_THAT(IsTrue(Character->ApplyAppearance(NotBaked)));
		FHitCandidate Candidate;
		ASSERT_THAT(IsTrue(LaunchAndFind(Candidate)));
		ASSERT_THAT(IsFalse(Stroke->PlayStroke(Candidate, ETennisShotType::Topspin)));
		ASSERT_THAT(IsFalse(Stroke->IsStroking() || Character->GetTennisMovement()->IsAutoMoving()));

		ASSERT_THAT(IsTrue(Character->ApplyAppearance(Baked)));
		ASSERT_THAT(IsTrue(Stroke->PlayStroke(Candidate, ETennisShotType::Topspin)));
	}

	// フォロースルーの途中で次のストロークを始めると、前のものは知らせずに打ち切り、フォロースルーの制限を外してから始める
	TEST_METHOD(PlayStroke_DuringFollowThroughCutsThePreviousStroke)
	{
		FRecord R;
		FHitCandidate Candidate;
		ASSERT_THAT(IsTrue(LaunchAndFind(Candidate) && Stroke->PlayStroke(Candidate, ETennisShotType::Topspin)));
		Record(R);
		for (int32 Frame = 0; Frame < 2000 && R.Impacts == 0; ++Frame)
		{
			TickFrame(Frames[Frame % Frames.Num()]);
		}
		ASSERT_THAT(IsTrue(Stroke->GetStepper().State == EStrokeState::FollowThrough));

		ASSERT_THAT(IsTrue(Stroke->PlayStroke(Candidate, ETennisShotType::Topspin)));  // 打点の時刻は過ぎている：インパクトの位置から
		ASSERT_THAT(IsTrue(Stroke->GetStepper().State == EStrokeState::Swinging));
		ASSERT_THAT(IsFalse(Character->GetTennisMovement()->GetModifiers().bFollowThrough));
		ASSERT_THAT(AreEqual(0, R.Finished));
		ASSERT_THAT(IsTrue(BodyFollowsTheStepper()));
	}
};

// 回したコート（ヨー 90°）の奥（+X の側）の選手：体の向きのコート局所 ↔ ワールドの変換と、ネットの方向（π）を確かめる（T-STRK-015）
TEST_CLASS(TennisStrokeComponentRotatedCourt, "Tennis.Game.Stroke.Component")
{
	FActorTestSpawner Spawner;
	ATennisCourt* Court = nullptr;
	ATennisBall* Ball = nullptr;
	ATennisCharacter* Character = nullptr;

	BEFORE_EACH()
	{
		Court = &Spawner.SpawnActorAt<ATennisCourt>(FVector(300.0, -200.0, 0.0), FRotator(0.0, 90.0, 0.0));
		Ball = &Spawner.SpawnActor<ATennisBall>();
		const FVector Local(11.5, -1.0, 0.9);  // 奥の選手（コート局所の m。カプセルの中心）
		Character = &Spawner.SpawnActorAt<ATennisCharacter>(Court->LocalMetersToWorld(Local), FRotator(0.0, 90.0 + 180.0, 0.0));
		Spawner.GetWorld().GetWorldSettings()->NotifyBeginPlay();
	}

	AFTER_EACH()
	{
		Spawner.GetWorld().EndPlay(EEndPlayReason::Quit);
	}

	/** 選手の体の向き（ワールド）をコート局所のヨー（rad）にする */
	double LocalYaw() const
	{
		const FVector Local = Court->GetActorTransform().InverseTransformVectorNoScale(Character->GetActorForwardVector());
		return FMath::Atan2(Local.Y, Local.X);
	}

	TEST_METHOD(FarSidePlayer_TurnsInCourtSpaceAndBackToTheNet)
	{
		FShotRequest Request;  // 手前（−X の側）から奥へ
		Request.Origin = FVector(-11.0, -0.5, 1.0);
		Request.Target = FVector(9.0, 1.0, 0.0);
		Request.ConstraintValue = 0.9;
		Request.MinSpeed = 18.0;
		Request.MaxSpeed = 40.0;
		Request.Rpm = 2000.0;
		Request.Time = Ball->GetSim()->GetState().Time;
		const FShotSolveResult Shot = FTennisShotSolver(Ball->GetSim()->GetContext()).Solve(Request);
		ASSERT_THAT(IsTrue(Shot.bSolved));
		Ball->GetSim()->Launch(Shot.Launch);
		FHitCandidate Candidate;
		ASSERT_THAT(IsTrue(Character->GetReach()->FindBest(Candidate)));
		UTennisStrokeComponent* Stroke = Character->GetStroke();
		ASSERT_THAT(IsTrue(Stroke->PlayStroke(Candidate, ETennisShotType::Topspin)));
		ASSERT_THAT(IsNear(UE_DOUBLE_PI, FMath::Abs(LocalYaw()), 1e-4));  // 始め：ネットの方向（−X）を向いている

		int32 Impacts = 0;
		bool bFinished = false;
		double ImpactTime = -1.0;
		Stroke->OnImpact.AddLambda([&Impacts, &ImpactTime](const FStrokeImpact& Impact) { ++Impacts; ImpactTime = Impact.Time; });
		Stroke->OnFinished.AddLambda([&bFinished]() { bFinished = true; });
		const double TargetYaw = Stroke->GetStepper().TargetYaw;
		for (int32 Frame = 0; Frame < 2000 && !bFinished; ++Frame)
		{
			Ball->GetSim()->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
			Stroke->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
			if (Stroke->IsStroking())
			{
				ASSERT_THAT(IsTrue(FMath::Abs(TennisStrokeTiming::WrapAngle(LocalYaw() - Stroke->GetStepper().Yaw)) < 1e-4));
			}
		}
		ASSERT_THAT(IsTrue(Impacts == 1 && bFinished));
		ASSERT_THAT(IsNear(Stroke->GetPlannedHit().Time, ImpactTime, 1e-9));
		ASSERT_THAT(IsTrue(FMath::Abs(TennisStrokeTiming::WrapAngle(TargetYaw - UE_DOUBLE_PI)) <= FMath::DegreesToRadians(35.0) + 1e-9));
		ASSERT_THAT(IsNear(0.0, TennisStrokeTiming::WrapAngle(LocalYaw() - UE_DOUBLE_PI), 1e-4));  // 終わり：ネットの方向（π）
	}
};
