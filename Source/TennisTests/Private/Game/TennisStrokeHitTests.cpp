// W-3.5、T-STRK-017：インパクトの判定と M3 の打球のつなぎこみ（DD-03 §6.4 の 4〜8）。打てば、T のボールの位置から時刻 T に、ニュートラルの
// 狙いへ打ち出して相手のコートに入る。空振り・球種のデータがないときは打ち出さない。どれも入力にはインパクトを知らせ、打球は知らせより先。
// ワールドは自動では進まないので、ボールとストロークだけを Tick する（選手の移動は動かさない：打つ位置へは置き直す）。
#include "Ball/TennisBall.h"
#include "Ball/TennisBallSimComponent.h"
#include "Components/ActorTestSpawner.h"
#include "Core/TennisPlayerSettings.h"
#include "Court/TennisCourt.h"
#include "CQTest.h"
#include "Data/ShotTypeData.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Input/TennisPlayerController.h"
#include "Input/TennisShotInputComponent.h"
#include "Misc/ScopeExit.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisMovementComponent.h"
#include "Player/TennisPlayerAppearance.h"
#include "Player/TennisStrokeComponent.h"
#include "Shot/TennisShotSolver.h"
#include "Stroke/TennisStrokeImpact.h"
#include "TennisConstants.h"
#include "TennisTestHelpers.h"

TEST_CLASS(TennisStrokeHit, "Tennis.Game.Stroke.Hit")
{
	FActorTestSpawner Spawner;
	ATennisCourt* Court = nullptr;
	ATennisBall* Ball = nullptr;
	ATennisCharacter* Character = nullptr;
	ATennisPlayerController* Controller = nullptr;

	FTrajectory Incoming;            // 打つ前のボールの予測（打ち出しの位置を比べる）
	TOptional<FStrokeImpact> Impact;  // ストロークが知らせたインパクト
	double FrameStartBallTime = 0.0;  // 今のフレームの始まりのボールの時刻
	double ImpactFrameStart = 0.0;    // インパクトのフレームの始まりのボールの時刻
	int32 Notified = 0;               // 選手が入力に知らせた回数
	bool bLaunchedBeforeNotify = false;
	FDelegateHandle ImpactHandle;
	FDelegateHandle NotifyHandle;

	BEFORE_EACH()
	{
		Court = &Spawner.SpawnActor<ATennisCourt>();  // 原点（コート局所 = ワールド。1 m = 100 cm）
		Ball = &Spawner.SpawnActor<ATennisBall>();
		Character = &Spawner.SpawnActorAt<ATennisCharacter>(FVector(-1150.0, 100.0, 90.0), FRotator::ZeroRotator);
		Controller = &Spawner.SpawnActor<ATennisPlayerController>();
		Spawner.GetWorld().GetWorldSettings()->NotifyBeginPlay();
		Controller->Possess(Character);
		Controller->GetShotInput()->BindInput(*NewObject<UEnhancedInputComponent>(Controller));

		// 相手（+X の側）からのラリーの球
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
		Incoming = Ball->GetSim()->GetPrediction();

		// 順は問わない（写すだけ）。知らせより先に打ったことは、選手の知らせ（下）で確かめる
		ImpactHandle = Character->GetStroke()->OnImpact.AddLambda([this](const FStrokeImpact& In)
		{
			Impact = In;
			ImpactFrameStart = FrameStartBallTime;
		});
		NotifyHandle = Character->OnStrokeImpact.AddLambda([this]()
		{
			++Notified;
			bLaunchedBeforeNotify = IsRelaunched();
		});
	}

	AFTER_EACH()
	{
		Character->GetStroke()->OnImpact.Remove(ImpactHandle);
		Character->OnStrokeImpact.Remove(NotifyHandle);
		Spawner.GetWorld().EndPlay(EEndPlayReason::Quit);
	}

	/** ボールの予測が、打つ前のものから変わった（打ち出した） */
	bool IsRelaunched() const
	{
		const TArray<FBallState>& Now = Ball->GetSim()->GetPrediction().States;
		return Now.Num() > 0 && Incoming.States.Num() > 0 && Now[0].Time != Incoming.States[0].Time;
	}

	/**
	 * トップスピンで押し（押したまま）、選手を打つ位置（選び直した打点の体の位置）から Offset（m）ずらして置き、インパクトの直後まで進める。
	 * AfterBallTick は、毎フレームのボールの Tick とストロークの Tick の間に呼ぶ。振り始められない・インパクトが来ないなら false
	 */
	bool SwingFrom(const FVector2D& Offset, TFunctionRef<void()> AfterBallTick = [] {})
	{
		Controller->GetShotInput()->OnShotPressed(0);  // トップスピン
		const FHitCandidate* Planned = Character->GetPlannedHit();
		if (Planned == nullptr || !Character->GetStroke()->IsStroking())
		{
			return false;
		}
		const FVector2D Body = Planned->BodyTarget + Offset;
		const FVector World = ATennisCourt::LocalMetersToWorld(Court->GetActorTransform(), FVector(Body.X, Body.Y, 0.0));
		Character->SetActorLocation(FVector(World.X, World.Y, Character->GetActorLocation().Z));
		const UTennisStrokeComponent* Stroke = Character->GetStroke();
		for (int32 Frame = 0; Frame < 2000 && Stroke->GetStepper().State != EStrokeState::FollowThrough; ++Frame)
		{
			FrameStartBallTime = Ball->GetSim()->GetState().Time;
			Ball->GetSim()->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
			AfterBallTick();
			Character->GetStroke()->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
		}
		return Impact.IsSet();
	}

	// 打つ位置で振れば打つ：打ち出しは T のボールの位置から時刻 T に、相手のコート（+X の側）のシングルスの中に落ちる。入力への知らせより先
	TEST_METHOD(InReach_LaunchesFromTheBallAtTIntoTheOpponentCourt)
	{
		ASSERT_THAT(IsTrue(SwingFrom(FVector2D::ZeroVector)));
		FBallState AtImpact;
		ASSERT_THAT(IsTrue(FTennisBallPredictor::GetStateAt(Incoming, Impact->Time, Ball->GetSim()->GetContext(), AtImpact)));
		ASSERT_THAT(IsTrue(FVector::Dist(AtImpact.Pos, Impact->SweetSpot) <= Character->GetStroke()->Sync.ImpactReach));

		const FTrajectory& Out = Ball->GetSim()->GetPrediction();
		ASSERT_THAT(IsTrue(Out.States.Num() > 0));
		ASSERT_THAT(IsNear(Impact->Time, Out.States[0].Time, 1e-9));
		ASSERT_THAT(IsTrue(Out.States[0].Pos.Equals(AtImpact.Pos, 1e-6)));
		ASSERT_THAT(IsTrue(Out.States[0].Vel.X > 0.0));
		const FBallEvent* Bounce = TennisTest::FirstOf(Out.Events, EBallEventType::Bounce);
		ASSERT_THAT(IsTrue(Bounce != nullptr));
		ASSERT_THAT(IsTrue(Bounce->Pos.X > 0.0 && Bounce->Pos.X <= TennisConst::HalfLength && FMath::Abs(Bounce->Pos.Y) <= TennisConst::HalfWidthSingles));

		ASSERT_THAT(AreEqual(1, Notified));
		ASSERT_THAT(IsTrue(bLaunchedBeforeNotify));
		ASSERT_THAT(IsTrue(Controller->GetShotInput()->GetState() == EShotInputState::Committed));  // 押したまま：インパクトで自動で離す
	}

	// 打つ位置から 1 m 離れていれば空振り：打ち出さないが、入力には知らせる
	TEST_METHOD(OutOfReach_DoesNotLaunchButNotifies)
	{
		ASSERT_THAT(IsTrue(SwingFrom(FVector2D(-1.0, 0.0))));
		ASSERT_THAT(IsFalse(IsRelaunched()));
		ASSERT_THAT(AreEqual(1, Notified));
		ASSERT_THAT(IsTrue(Controller->GetShotInput()->GetState() == EShotInputState::Committed));
	}

	// 押した球種のデータが設定にない：打ち出さず（警告）、入力には知らせる
	TEST_METHOD(NoShotTypeData_DoesNotLaunchButNotifies)
	{
		UTennisPlayerSettings* Settings = GetMutableDefault<UTennisPlayerSettings>();
		const TArray<TSoftObjectPtr<UShotTypeData>> Saved = Settings->ShotTypes;
		ON_SCOPE_EXIT { Settings->ShotTypes = Saved; };
		Settings->ShotTypes.Reset();
		ASSERT_THAT(IsTrue(SwingFrom(FVector2D::ZeroVector)));
		ASSERT_THAT(IsFalse(IsRelaunched()));
		ASSERT_THAT(AreEqual(1, Notified));
	}

	// T のボールの状態が求まらない（インパクトの前に、今の時刻から打ち出し直した：予測が T より後から始まる）：打たず、入力には知らせる
	TEST_METHOD(NoBallStateAtT_DoesNotLaunchButNotifies)
	{
		UTennisBallSimComponent* Sim = Ball->GetSim();
		ASSERT_THAT(IsTrue(SwingFrom(FVector2D::ZeroVector, [Sim]()
		{
			FBallState Now = Sim->GetState();
			Now.Time += 1e-6;  // 予測の始まりを今より少し後に（インパクトの時刻 T は今以前）
			Sim->Launch(Now);
		})));
		ASSERT_THAT(IsTrue(Sim->GetState().Vel.X < 0.0));  // 打ち返していない（相手から来る向きのまま）
		ASSERT_THAT(AreEqual(1, Notified));
		ASSERT_THAT(IsTrue(Controller->GetShotInput()->GetState() == EShotInputState::Committed));
	}

	// T のスイートスポット（DD-03 §6.4 の 5）：フレームの始まりの位置 + 速度（cm/s をコート局所の m/s に）×（T − フレームの始まり）の体の、
	// TargetYaw と焼いた値の位置（移動は Tick しないので、置いた速度がそのまま残る）
	TEST_METHOD(Impact_SweetSpotFollowsTheBodyMovingDuringTheFrame)
	{
		UTennisMovementComponent* Movement = Character->GetTennisMovement();
		ASSERT_THAT(IsTrue(SwingFrom(FVector2D::ZeroVector, [Movement]() { Movement->Velocity = FVector(300.0, -200.0, 0.0); })));
		const double Lead = Impact->Time - ImpactFrameStart;
		ASSERT_THAT(IsTrue(Lead > 0.0 && Lead <= 1.0 / 60.0 + 1e-9));
		const FVector Start = Court->WorldToLocalMeters(Character->GetActorLocation());
		const FVector2D Body = FVector2D(Start.X, Start.Y) + FVector2D(3.0, -2.0) * Lead;
		const FTennisStrokeFit* Fit = Character->GetAppearance()->FindStrokeFit(Character->GetStroke()->GetStroke());
		ASSERT_THAT(IsTrue(Fit != nullptr));
		const FVector Expected = TennisStrokeImpact::SweetSpotAt(Body, Character->GetStroke()->GetStepper().TargetYaw,
			TennisStrokeImpact::HandedOffset(Fit->SweetSpotOffset.GetLocation(), ETennisHandedness::Right));
		ASSERT_THAT(IsTrue(Impact->SweetSpot.Equals(Expected, 1e-9)));
	}

	// 設定には、どの球種にもその球種のデータがある（押した球種で打てる）
	TEST_METHOD(Settings_HaveShotDataForEveryShotType)
	{
		const UTennisPlayerSettings* Settings = GetDefault<UTennisPlayerSettings>();
		const UEnum* Types = StaticEnum<ETennisShotType>();
		for (int32 Index = 0; Index < Types->NumEnums() - 1; ++Index)  // 最後は _MAX
		{
			const ETennisShotType Type = static_cast<ETennisShotType>(Types->GetValueByIndex(Index));
			const UShotTypeData* Data = Settings->FindShotType(Type);
			ASSERT_THAT(IsTrue(Data != nullptr && Data->Type == Type, *FString::Printf(TEXT("no DA_Shot_* for %s"), *Types->GetNameStringByIndex(Index))));
		}
	}
};
