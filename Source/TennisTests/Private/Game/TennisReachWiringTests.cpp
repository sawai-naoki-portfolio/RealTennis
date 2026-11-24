// W-3.3：打点の探索を選手と入力につなぐ（DD-03 §4.4、ST-05）。押した瞬間に候補が見つかれば、同じ呼び出しのうちに溜め中になる。
#include "Ball/TennisBall.h"
#include "Ball/TennisBallSimComponent.h"
#include "Components/ActorTestSpawner.h"
#include "Court/TennisCourt.h"
#include "CQTest.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Input/TennisPlayerController.h"
#include "Input/TennisShotInputComponent.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisMovementComponent.h"
#include "Player/TennisReachComponent.h"
#include "Player/TennisStrokeComponent.h"
#include "Misc/ScopeExit.h"
#include "Shot/TennisShotSolver.h"
#include "TennisGameTestHelpers.h"
#include "TennisTestHelpers.h"

TEST_CLASS(TennisReachWiring, "Tennis.Game.Player.Reach")
{
	FActorTestSpawner Spawner;
	ATennisBall* Ball = nullptr;
	ATennisCharacter* Character = nullptr;
	ATennisPlayerController* Controller = nullptr;

	BEFORE_EACH()
	{
		Spawner.SpawnActor<ATennisCourt>();  // 原点（コート局所 = ワールド。1 m = 100 cm）
		Ball = &Spawner.SpawnActor<ATennisBall>();
		Character = &Spawner.SpawnActorAt<ATennisCharacter>(FVector(-1150.0, 100.0, 90.0), FRotator::ZeroRotator);
		Controller = &Spawner.SpawnActor<ATennisPlayerController>();
		Spawner.GetWorld().GetWorldSettings()->NotifyBeginPlay();  // コート・ボールを見つける（移動の側・処理の順・探索の対象）
		Controller->Possess(Character);
		Controller->GetShotInput()->BindInput(*NewObject<UEnhancedInputComponent>(Controller));
	}

	AFTER_EACH()
	{
		Character->OnStrokeEnded.Remove(EndedHandle);
		Spawner.GetWorld().EndPlay(EEndPlayReason::Quit);  // 始めたプレイを終えてから、ワールドを片づける
	}

	/** 相手（+X の側）からのラリーの球を打ち出す */
	void LaunchRallyBall()
	{
		FShotRequest Request;
		Request.Origin = FVector(11.0, 0.5, 1.0);
		Request.Target = FVector(-9.0, -1.0, 0.0);
		Request.ConstraintValue = 0.9;
		Request.MinSpeed = 18.0;
		Request.MaxSpeed = 40.0;
		Request.Rpm = 2000.0;
		Request.Time = Ball->GetSim()->GetState().Time;
		const FShotSolveResult Shot = FTennisShotSolver(Ball->GetSim()->GetContext()).Solve(Request);
		ASSERT_THAT(IsTrue(Shot.bSolved));
		Ball->GetSim()->Launch(Shot.Launch);
	}

	// 探索の問い合わせ（DD-03 §4.1）：選手のコート局所の位置（m）・側・今のボールの時刻。打点の選び直し（§6.2）も同じものを使う
	TEST_METHOD(MakeQuery_UsesThePlayerAndTheBallClock)
	{
		LaunchRallyBall();
		FReachQuery Q;
		ASSERT_THAT(IsTrue(Character->GetReach()->MakeQuery(Q)));
		ASSERT_THAT(IsTrue(Q.Position.Equals(FVector2D(-11.5, 1.0), 1e-6)));  // 置いた位置（cm）をコート局所の m に
		ASSERT_THAT(IsNear(-1.0, Q.SideSign, 0.0));
		ASSERT_THAT(IsTrue(Q.Now == Ball->GetSim()->GetState().Time));
		ASSERT_THAT(IsNear(Character->GetReach()->Params.DefaultPrepTime, Q.PrepTime, 0.0));

		UTennisReachComponent* Unattached = NewObject<UTennisReachComponent>();  // 選手もボールもない
		ASSERT_THAT(IsFalse(Unattached->MakeQuery(Q)));
	}

	/** OnStrokeEnded の回数を数える（テストの終わりで外す） */
	int32 Ended = 0;
	FDelegateHandle EndedHandle;

	void CountEnded()
	{
		Ended = 0;
		EndedHandle = Character->OnStrokeEnded.AddLambda([this]() { ++Ended; });
	}

	/** ボール → ストロークの順に 1/60 s ずつ Tick する（ワールドは自動では進まない）。Done になるまで（最大 2000 フレーム） */
	template <typename FDone>
	void TickUntil(FDone Done)
	{
		for (int32 Frame = 0; Frame < 2000 && !Done(); ++Frame)
		{
			Ball->GetSim()->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
			Character->GetStroke()->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
		}
	}

	// 球が来ていれば、押した瞬間に候補が見つかり、入力は溜め中になり（同じ呼び出しのうち）、ストロークを振り始める（DD-03 §6.6）。
	// 離しても、ストロークは終わらない（終わりは RecoverStart）。入力は離した状態で、離した時刻を選手が覚える
	TEST_METHOD(PressWithBallComing_ChargesAtOnceAndStartsTheStroke)
	{
		LaunchRallyBall();
		UTennisShotInputComponent* ShotInput = Controller->GetShotInput();
		ShotInput->OnShotPressed(0);  // トップスピン

		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Charging));
		const FHitCandidate* Planned = Character->GetPlannedHit();
		ASSERT_THAT(IsTrue(Planned != nullptr && Planned->Kind == EHitKind::Groundstroke && !Planned->bStretch));
		ASSERT_THAT(IsTrue(Character->GetShotIntent() != nullptr && Character->GetShotIntent()->ChargeStartTime.IsSet()));
		ASSERT_THAT(IsFalse(Character->GetReach()->IsSearching()));
		ASSERT_THAT(IsTrue(Character->GetStroke()->IsStroking()));
		ASSERT_THAT(IsTrue(Planned->Time == Character->GetStroke()->GetPlannedHit().Time));  // 選び直した打点

		ShotInput->OnShotReleased(0);
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Released));
		ASSERT_THAT(IsTrue(Character->GetStroke()->IsStroking() && Character->GetPlannedHit() != nullptr));
		ASSERT_THAT(IsTrue(Character->GetShotIntent() != nullptr && Character->GetShotIntent()->ReleaseTime.IsSet()));
	}

	// インパクトの前に離す：インパクトで確定し、RecoverStart で待機に戻る（ST-05）。自動移動はインパクトで止まる
	TEST_METHOD(ReleasedBeforeTheImpact_CommitsAtTheImpactAndEndsAtRecoverStart)
	{
		LaunchRallyBall();
		UTennisShotInputComponent* ShotInput = Controller->GetShotInput();
		CountEnded();
		ShotInput->OnShotPressed(0);
		ShotInput->OnShotReleased(0);
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Released));
		const UTennisStrokeComponent* Stroke = Character->GetStroke();
		TickUntil([Stroke]() { return Stroke->GetStepper().State == EStrokeState::FollowThrough; });  // インパクトの直後
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Committed));
		ASSERT_THAT(IsFalse(Character->GetTennisMovement()->IsAutoMoving()));
		TickUntil([ShotInput]() { return ShotInput->GetState() == EShotInputState::Idle; });
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Idle && !Stroke->IsStroking()));
		ASSERT_THAT(IsTrue(Character->GetShotIntent() == nullptr && Character->GetPlannedHit() == nullptr));
		ASSERT_THAT(IsFalse(Character->GetTennisMovement()->IsAutoMoving() || Character->GetTennisMovement()->GetModifiers().bFollowThrough));
		ASSERT_THAT(AreEqual(1, Ended));  // 終わりの知らせは 1 回
	}

	// 押したまま：インパクトで自動で離した扱いになって確定し（操作仕様 §4.1）、RecoverStart で待機に戻り、次の押下が効く（DD-03 §6.6）
	TEST_METHOD(HeldThroughTheStroke_EndsAtRecoverStartAndAcceptsTheNextPress)
	{
		LaunchRallyBall();
		UTennisShotInputComponent* ShotInput = Controller->GetShotInput();
		CountEnded();
		ShotInput->OnShotPressed(0);
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Charging && Character->GetStroke()->IsStroking()));
		const UTennisStrokeComponent* Stroke = Character->GetStroke();
		TickUntil([Stroke]() { return Stroke->GetStepper().State == EStrokeState::FollowThrough; });  // インパクトの直後
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Committed));  // 押したまま：インパクトで自動で離した扱いで確定
		ASSERT_THAT(IsTrue(Character->GetShotIntent() != nullptr && Character->GetShotIntent()->bAutoReleased));
		TickUntil([ShotInput]() { return ShotInput->GetState() == EShotInputState::Idle; });
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Idle && !Stroke->IsStroking()));
		ASSERT_THAT(AreEqual(1, Ended));

		LaunchRallyBall();  // 次の球
		ShotInput->OnShotPressed(0);
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Charging && Character->GetStroke()->IsStroking()));
	}

	// ストロークを選べない・始められない（アニメのインスタンスがない）：溜め中から待機に戻り、終わりを 1 回知らせる（自動移動もしない）
	TEST_METHOD(StrokeCannotStart_ReturnsToIdle)
	{
		LaunchRallyBall();
		Character->GetMesh()->SetAnimInstanceClass(nullptr);
		UTennisShotInputComponent* ShotInput = Controller->GetShotInput();
		CountEnded();
		ShotInput->OnShotPressed(0);
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Idle && !Character->GetStroke()->IsStroking()));
		ASSERT_THAT(IsTrue(Character->GetPlannedHit() == nullptr && !Character->GetTennisMovement()->IsAutoMoving()));
		ASSERT_THAT(AreEqual(1, Ended));
	}

	// インパクトの後に離す：インパクトで確定済み（押したまま）なので何も起きず、ストロークは続き、RecoverStart で待機に戻る。終わりの知らせは 1 回
	TEST_METHOD(ReleasedAfterTheImpact_ChangesNothingAndEndsAtRecoverStart)
	{
		LaunchRallyBall();
		UTennisShotInputComponent* ShotInput = Controller->GetShotInput();
		CountEnded();
		ShotInput->OnShotPressed(0);
		const UTennisStrokeComponent* Stroke = Character->GetStroke();
		TickUntil([Stroke]() { return Stroke->GetStepper().State == EStrokeState::FollowThrough; });
		ShotInput->OnShotReleased(0);
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Committed && Stroke->IsStroking() && Ended == 0));
		ASSERT_THAT(IsTrue(Character->GetShotIntent() != nullptr && !Character->GetShotIntent()->ReleaseTime.IsSet()));  // 離した時刻は記録しない（インパクトで決まっている）
		TickUntil([ShotInput]() { return ShotInput->GetState() == EShotInputState::Idle; });
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Idle && !Stroke->IsStroking()));
		ASSERT_THAT(AreEqual(1, Ended));
	}

	// インパクトの後に取り消すと、ストロークは終わりまで進むが、その終わりは次の予約を消さない（押し直したバッファ中の探索が続く）
	TEST_METHOD(CanceledAfterTheImpact_KeepsTheNextReservation)
	{
		LaunchRallyBall();
		UTennisShotInputComponent* ShotInput = Controller->GetShotInput();
		CountEnded();
		ShotInput->OnShotPressed(0);
		const UTennisStrokeComponent* Stroke = Character->GetStroke();
		TickUntil([Stroke]() { return Stroke->GetStepper().State == EStrokeState::FollowThrough; });
		ShotInput->Cancel();  // 購読は残したまま取り消す（入力の段階の変更と同じ）
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Idle && Stroke->IsStroking()));

		FBallState Away;  // 相手のコートに止まっている球：押し直しても候補はない
		Away.Pos = FVector(8.0, 0.0, FBallParams().Radius);
		Away.Time = Ball->GetSim()->GetState().Time;
		Ball->GetSim()->Launch(Away);
		ShotInput->OnShotPressed(0);
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Buffered && Character->GetReach()->IsSearching()));
		TickUntil([Stroke]() { return !Stroke->IsStroking(); });  // 前のストロークの RecoverStart（入力のバッファの時間切れより前）
		ASSERT_THAT(IsFalse(Stroke->IsStroking()));
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Buffered && Character->GetReach()->IsSearching()));
		ASSERT_THAT(IsTrue(Character->GetShotIntent() != nullptr));
		ASSERT_THAT(AreEqual(0, Ended));  // 取り消したストロークの終わりは知らせない
	}

	// 長いフレーム（時間の倍率 8、0.1 s）で、インパクトと RecoverStart が同じ Tick に来ても、終わりの知らせは 1 回（インパクトで確定してから待機）
	TEST_METHOD(ImpactAndRecoverStartInOneTick_EndOnce)
	{
		RunImpactAndRecoverStartInOneTick(true);
	}

	// 押したまま：インパクトの知らせの中で自動で離した扱いの確定（SubmitShotIntent）、続く RecoverStart で待機。終わりの知らせは 1 回
	TEST_METHOD(ImpactAndRecoverStartInOneTick_HeldEndsOnce)
	{
		RunImpactAndRecoverStartInOneTick(false);
	}

	void RunImpactAndRecoverStartInOneTick(bool bRelease)
	{
		const float SavedTimeScale = GetDefault<UTennisDebugSettings>()->GetTimeScale();
		ON_SCOPE_EXIT
		{
			TennisTest::SetTimeScale(SavedTimeScale);
		};
		TennisTest::SetTimeScale(1.0f);
		LaunchRallyBall();
		UTennisShotInputComponent* ShotInput = Controller->GetShotInput();
		CountEnded();
		ShotInput->OnShotPressed(0);
		if (bRelease)
		{
			ShotInput->OnShotReleased(0);
		}
		const UTennisStrokeComponent* Stroke = Character->GetStroke();
		// インパクトの直前まで（モンタージュの位置でインパクトの 0.05 s 前）
		TickUntil([Stroke]()
		{
			return Stroke->GetStepper().State == EStrokeState::Swinging && Stroke->GetStepper().Clip.ImpactTime - Stroke->GetStepper().Position < 0.05;
		});
		ASSERT_THAT(IsTrue(Stroke->GetStepper().State == EStrokeState::Swinging));
		TennisTest::SetTimeScale(8.0f);  // 1 フレーム 0.1 s × 8 = 0.8 s：残り < 0.0625 s とフォロースルー ≤ 0.625 s を 1 回で越える
		Ball->GetSim()->TickComponent(0.1f, LEVELTICK_All, nullptr);
		Character->GetStroke()->TickComponent(0.1f, LEVELTICK_All, nullptr);
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Idle && !Stroke->IsStroking()));
		ASSERT_THAT(AreEqual(1, Ended));
	}

	// T-MOVE-006：候補が見つかると、選び直した打点に入る位置へ自動移動を始める。離しても続き（ストロークが続く）、取り消す（ポーンを外す）と、
	// インパクトの前なのでストロークと一緒に止まる（DD-03 §2.3・§6.6）
	TEST_METHOD(CandidateStartsAutoMove_ReleaseKeepsItAndCancelStopsIt)
	{
		LaunchRallyBall();
		UTennisShotInputComponent* ShotInput = Controller->GetShotInput();
		const UTennisMovementComponent* Movement = Character->GetTennisMovement();
		ASSERT_THAT(IsFalse(Movement->IsAutoMoving()));
		ShotInput->OnShotPressed(0);
		ASSERT_THAT(IsTrue(Character->GetPlannedHit() != nullptr && Movement->IsAutoMoving()));
		ShotInput->OnShotReleased(0);
		ASSERT_THAT(IsTrue(Movement->IsAutoMoving()));
		Controller->UnPossess();  // 入力を取り消す（CancelShotIntent）
		ASSERT_THAT(IsTrue(Character->GetPlannedHit() == nullptr));
		ASSERT_THAT(IsFalse(Movement->IsAutoMoving() || Character->GetStroke()->IsStroking()));
	}

	// 球が来ていなければバッファで待ち、探し続ける。離すと探すのをやめる
	TEST_METHOD(PressWithoutBall_BuffersAndKeepsSearching)
	{
		UTennisShotInputComponent* ShotInput = Controller->GetShotInput();
		ShotInput->OnShotPressed(0);
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Buffered));
		ASSERT_THAT(IsTrue(Character->GetPlannedHit() == nullptr));
		ASSERT_THAT(IsTrue(Character->GetReach()->IsSearching()));

		ShotInput->OnShotReleased(0);
		ASSERT_THAT(IsTrue(ShotInput->GetState() == EShotInputState::Idle));
		ASSERT_THAT(IsFalse(Character->GetReach()->IsSearching()));
	}
};
