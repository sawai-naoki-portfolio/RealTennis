// W-3.5、T-STRK-003・004：1,000 打のインパクトの距離（DD-03 §12.1）と、インパクトの時刻の誤差（§12.2。同じ打で測る）。打球ツールの打ち方で選手の側へ球を出し、押した瞬間の入力を直に渡して、
// 自動移動・ストロークの時計・打点の選び直しが、スイートスポットをボールに合わせるかを測る（平均 < 5 cm）。数える打は、押した瞬間の候補が
// Groundstroke で、選び直した打点の高さが 0.7 m 以上 1.3 m 未満の打（M3 の対象。数字で決める）。打ごとに、候補と選び直した打点の高さと、
// 選ばれたストロークをログに出す（高さの変種の境界が効いているかを見る）。時刻の誤差は、インパクトの時刻 T と選び直した打点の時刻の差で、
// すべての打で 1 フレーム（1/60 s）未満。すぐ押す打はどれも開始を遅らせる始め方になるので、押す時刻を 0〜60 フレーム遅らせた打でも、
// 3 つの始め方がそれぞれ 50 打以上になるまで測る（距離は T-STRK-003 に入れない）。どちらも同じシードで 2 回走らせ、ビットまで同じ結果に
// なることも確かめる。
#include "Ball/TennisBall.h"
#include "Ball/TennisBallSimComponent.h"
#include "Components/ActorTestSpawner.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TennisPlayerSettings.h"
#include "Court/TennisCourt.h"
#include "CQTest.h"
#include "Data/ShotTypeData.h"
#include "Data/StrokeData.h"
#include "Debug/TennisShotTester.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Misc/ScopeExit.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisMovementComponent.h"
#include "Player/TennisStrokeComponent.h"
#include "Stroke/TennisStrokeImpact.h"
#include "Stroke/TennisStrokeStepper.h"
#include "TennisGameTestHelpers.h"
#include "TennisSimLog.h"

namespace
{
	constexpr int32 CountedHits = 1000;        // T-STRK-003
	constexpr int32 MaxBalls = 4000;           // 数える打が足りないまま、これだけ出したら失敗
	constexpr int32 Seed = 20261118;
	constexpr double MaxMeanDistance = 0.05;   // m（平均 < 5 cm：DD-03 §12）
	constexpr double CountedMinHeight = 0.7;   // m（数える打の、選び直した打点の高さ：DD-03 §12.1。下限を含む）
	constexpr double CountedMaxHeight = 1.3;   // m（上限を含まない）
	constexpr double MaxTimeError = 1.0 / 60.0; // s（60 fps の 1 フレーム。すべての打で：DD-03 §12.2）
	constexpr int32 MaxPressWaitFrames = 60;   // 押す時刻を遅らせた打：打ち出してから押すまで 0〜60 フレーム（§12.2 の (2)）
	constexpr int32 MinHitsPerStart = 50;      // (2) で、3 つの始め方のそれぞれに要る打の数
	constexpr float FrameTime = 1.0f / 60.0f;  // s（固定）
	constexpr int32 MaxFramesPerBall = 600;
	constexpr int32 MaxSettleFrames = 300;     // 球の間に止まるまで待つ上限（止まらなければ失敗）
	const FVector2D StartPosition(-12.0, 0.0); // m（コート局所。球ごとにここから、止まった状態で始める）

	struct FMeasure
	{
		TArray<double> Distances;  // m（数えた打。当たらなかった打も含む）
		TArray<double> TimeErrors; // s（数えた打の |T − 選び直した打点の時刻|）
		int32 InRangeStarts = 0;   // 始め方（§6.1）：範囲の中の速度
		int32 DelayedStarts = 0;   // 開始を遅らせた
		int32 ShortenedStarts = 0; // 途中から再生した（テイクバックを省いた）
		double HorizontalSum = 0.0;
		double VerticalSum = 0.0;  // 高さの成分の絶対値
		int32 Balls = 0;
		int32 NoCandidate = 0;
		int32 NotGroundstroke = 0;
		int32 OutOfHeight = 0;
		int32 Misses = 0;          // 数えた打のうち、届く距離（FStrokeSyncParams::ImpactReach）を超えた打
		FString Failure;           // 数える打でインパクトが来なかった・ボールの状態が求まらなかった
	};

	/** 押す時刻：すぐ（T-STRK-003・004 の (1)）か、遅らせる（T-STRK-004 の (2)）か */
	enum class EPress : uint8
	{
		Immediately,
		Late,
	};

	/** 測り終えたか：すぐ押すなら数える打が CountedHits、遅らせるなら 3 つの始め方がそれぞれ MinHitsPerStart */
	bool IsDone(const FMeasure& M, EPress Press)
	{
		return Press == EPress::Immediately ? M.Distances.Num() >= CountedHits
			: FMath::Min3(M.InRangeStarts, M.DelayedStarts, M.ShortenedStarts) >= MinHitsPerStart;
	}

	double MeanOf(const TArray<double>& Values)
	{
		double Sum = 0.0;
		for (const double V : Values)
		{
			Sum += V;
		}
		return Values.Num() > 0 ? Sum / Values.Num() : 0.0;
	}

	/** 最大。数でない値があれば無限大（比べると必ず落ちる） */
	double MaxOf(const TArray<double>& Values)
	{
		double Max = 0.0;
		for (const double V : Values)
		{
			if (!FMath::IsFinite(V))
			{
				return TNumericLimits<double>::Max();
			}
			Max = FMath::Max(Max, V);
		}
		return Max;
	}

	/** ビットまで同じ（決定論。== は -0.0 と 0.0 を同じとみなす） */
	bool BitEqual(const TArray<double>& A, const TArray<double>& B)
	{
		return A.Num() == B.Num() && FMemory::Memcmp(A.GetData(), B.GetData(), A.Num() * sizeof(double)) == 0;
	}

	bool SameResult(const FMeasure& A, const FMeasure& B)
	{
		return BitEqual(A.Distances, B.Distances) && BitEqual(A.TimeErrors, B.TimeErrors) && A.Balls == B.Balls;
	}

	/** 足元の当たりのある床（上の面が z = 0。コートのアクタは見た目と当たりを持たない） */
	void SpawnFloor(FActorTestSpawner& Spawner)
	{
		AStaticMeshActor& Floor = Spawner.SpawnActorAt<AStaticMeshActor>(FVector(0.0, 0.0, -50.0), FRotator::ZeroRotator);
		UStaticMeshComponent* Mesh = Floor.GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Floor.SetActorScale3D(FVector(60.0, 30.0, 1.0));  // 1 m の立方体 → 60 × 30 × 1 m
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	}

	/** 本番の Tick の順（ボール → ストローク → 選手の移動：ATennisCharacter::FollowBall）で 1 フレーム進める */
	void TickFrame(ATennisBall& Ball, ATennisCharacter& Character)
	{
		Ball.GetSim()->TickComponent(FrameTime, LEVELTICK_All, nullptr);
		Character.GetStroke()->TickComponent(FrameTime, LEVELTICK_All, nullptr);
		Character.GetCharacterMovement()->TickComponent(FrameTime, LEVELTICK_All, nullptr);
	}

	/** シード Seed で、測り終えるまで（IsDone）球を出して測る（新しいワールドで）。bLogHits なら、数えた打ごとにログに出す */
	FMeasure Run(EPress Press, bool bLogHits)
	{
		// ボールと移動の刻みは、開発者ごとの時間の倍率（tennis.TimeScale。撮影の値が残ることがある）を読むので、1 にして測る
		const float SavedTimeScale = GetDefault<UTennisDebugSettings>()->GetTimeScale();
		TennisTest::SetTimeScale(1.0f);
		ON_SCOPE_EXIT
		{
			TennisTest::SetTimeScale(SavedTimeScale);
		};
		FMeasure Out;
		FActorTestSpawner Spawner;
		const ATennisCourt& Court = Spawner.SpawnActor<ATennisCourt>();
		SpawnFloor(Spawner);
		ATennisBall& Ball = Spawner.SpawnActor<ATennisBall>();
		ATennisCharacter& Character = Spawner.SpawnActorAt<ATennisCharacter>(
			Court.LocalMetersToWorld(FVector(StartPosition.X, StartPosition.Y, 1.0)), FRotator::ZeroRotator);
		Spawner.GetWorld().GetWorldSettings()->NotifyBeginPlay();
		UCharacterMovementComponent* Movement = Character.GetCharacterMovement();
		Movement->bRunPhysicsWithNoController = true;  // 操作する人なしで動かす（テストのプレイヤーコントローラーはローカルの操作にならない）
		Movement->SetMovementMode(MOVE_Walking);

		const UTennisPlayerSettings* Settings = GetDefault<UTennisPlayerSettings>();
		const UTennisStrokeComponent* Stroke = Character.GetStroke();
		FRandomStream Random(Seed);
		FRandomStream PressRandom(Seed);  // 押すまで待つフレーム（§12.2 の (2)。別の流れにして、Random の並びを変えない）
		TOptional<FStrokeImpact> Impact;
		const FDelegateHandle Handle = Character.GetStroke()->OnImpact.AddLambda([&Impact](const FStrokeImpact& In) { Impact = In; });
		// 候補が見つかった知らせの時点では、予約の打点は候補のまま（選び直す前：ATennisCharacter::HandleCandidateFound）。
		// ストロークを始められないと、予約の打点は消える（候補なしと区別するために、見つかったことを覚えておく）
		bool bCandidateFound = false;
		double CandidateHeight = 0.0;
		const FDelegateHandle FoundHandle = Character.OnHitCandidateFound.AddLambda([&Character, &bCandidateFound, &CandidateHeight]()
		{
			const FHitCandidate* Found = Character.GetPlannedHit();
			bCandidateFound = true;
			CandidateHeight = Found != nullptr ? Found->BallPos.Z : 0.0;
		});
		ON_SCOPE_EXIT
		{
			Character.GetStroke()->OnImpact.Remove(Handle);
			Character.OnHitCandidateFound.Remove(FoundHandle);
			Spawner.GetWorld().EndPlay(EEndPlayReason::Quit);
		};

		while (!IsDone(Out, Press) && Out.Balls < MaxBalls && Out.Failure.IsEmpty())
		{
			++Out.Balls;
			// 止まるまで進めてから、決まった位置に置き直す
			const auto IsBusy = [&]() { return Stroke->IsStroking() || !Character.GetTennisMovement()->GetMoveState().Velocity.IsNearlyZero(1e-4); };
			for (int32 Frame = 0; Frame < MaxSettleFrames && IsBusy(); ++Frame)
			{
				TickFrame(Ball, Character);
			}
			if (IsBusy())
			{
				Out.Failure = FString::Printf(TEXT("ball %d: the player did not settle in %d frames"), Out.Balls, MaxSettleFrames);
				break;
			}
			const FVector Start = Court.LocalMetersToWorld(FVector(StartPosition.X, StartPosition.Y, 0.0));
			Character.SetActorLocation(FVector(Start.X, Start.Y, Character.GetActorLocation().Z));

			const ETennisShotType Type = static_cast<ETennisShotType>(Random.RandRange(0, static_cast<int32>(ETennisShotType::Smash)));  // ラリーの 10 種
			const UShotTypeData* Shot = Settings->FindShotType(Type);
			const FVector From(11.0, Random.FRandRange(-2.0, 2.0), 1.0);
			const FVector Target(Random.FRandRange(-10.0, -5.0), Random.FRandRange(-3.5, 3.5), 0.0);
			const double Charge = Random.FRand();
			if (Shot == nullptr)
			{
				Out.Failure = FString::Printf(TEXT("no shot type data for type %d"), static_cast<int32>(Type));
				break;
			}
			ATennisShotTester::Shoot(*Ball.GetSim(), Shot->Params, From, Target, Charge);
			const FTrajectory Incoming = Ball.GetSim()->GetPrediction();
			Impact.Reset();
			const int32 WaitFrames = Press == EPress::Late ? PressRandom.RandRange(0, MaxPressWaitFrames) : 0;
			for (int32 Frame = 0; Frame < WaitFrames; ++Frame)
			{
				TickFrame(Ball, Character);
			}
			bCandidateFound = false;
			Character.SubmitShotIntent(FShotIntent{ ETennisShotType::Topspin });  // 押した瞬間（押したまま離さない）

			const FHitCandidate* Planned = Character.GetPlannedHit();  // 選び直した打点
			if (bCandidateFound && Planned == nullptr)
			{
				// どの候補にもストロークがある（T-STRK-002）。始まらないのは、表・焼いた値・モンタージュの抜け
				Out.Failure = FString::Printf(TEXT("ball %d (type %d): no stroke started for a candidate z=%.4f (see LogTennisStroke)"),
					Out.Balls, static_cast<int32>(Type), CandidateHeight);
				break;
			}
			const bool bCounted = Planned != nullptr && Planned->Kind == EHitKind::Groundstroke
				&& Planned->BallPos.Z >= CountedMinHeight && Planned->BallPos.Z < CountedMaxHeight;
			if (!bCounted)
			{
				Planned == nullptr ? ++Out.NoCandidate : Planned->Kind != EHitKind::Groundstroke ? ++Out.NotGroundstroke : ++Out.OutOfHeight;
				Character.CancelShotIntent();
				continue;
			}
			const double HitTime = Planned->Time;  // ストロークが合わせる打点の時刻（ボールの時刻）
			const FStrokeStepper& Started = Stroke->GetStepper();  // 始めた直後（§12.2 の始め方）
			Started.Delay > 0.0 ? ++Out.DelayedStarts : Started.StartPosition > 0.0 ? ++Out.ShortenedStarts : ++Out.InRangeStarts;
			for (int32 Frame = 0; Frame < MaxFramesPerBall && !Impact.IsSet(); ++Frame)
			{
				TickFrame(Ball, Character);
			}
			FBallState AtImpact;
			if (!Impact.IsSet() || !FTennisBallPredictor::GetStateAt(Incoming, Impact->Time, Ball.GetSim()->GetContext(), AtImpact))
			{
				Out.Failure = FString::Printf(TEXT("ball %d (type %d): %s"), Out.Balls, static_cast<int32>(Type),
					Impact.IsSet() ? TEXT("no ball state at the impact") : TEXT("no impact"));
				break;
			}
			const FVector Gap = AtImpact.Pos - Impact->SweetSpot;
			Out.Distances.Add(Gap.Size());
			Out.TimeErrors.Add(FMath::Abs(Impact->Time - HitTime));
			Out.Misses += TennisStrokeImpact::IsWithinReach(Gap.Size(), Stroke->Sync) ? 0 : 1;
			if (bLogHits)
			{
				UE_LOG(LogTennisStroke, Log, TEXT("[Measure] hit=%d ball=%d type=%d stroke=%s candidate_z=%.4f refined_z=%.4f distance=%.4f vertical=%.4f time_error=%.3e"),
					Out.Distances.Num(), Out.Balls, static_cast<int32>(Type), *GetNameSafe(Stroke->GetStroke()), CandidateHeight, Planned->BallPos.Z,
					Gap.Size(), Gap.Z, Out.TimeErrors.Last());
			}
			Out.HorizontalSum += FVector2D(Gap.X, Gap.Y).Size();
			Out.VerticalSum += FMath::Abs(Gap.Z);
			Character.CancelShotIntent();
		}
		return Out;
	}
}

TEST_CLASS(TennisStrokeMeasure, "Tennis.Game.Stroke.Measure")
{
	TEST_METHOD(ImpactOf1000Hits_IsWithin5cmAndOneFrame)
	{
		const FMeasure First = Run(EPress::Immediately, true);
		ASSERT_THAT(IsTrue(First.Failure.IsEmpty(), *First.Failure));
		ASSERT_THAT(AreEqual(CountedHits, First.Distances.Num(), TEXT("not enough counted hits (DD-03 §12.1)")));

		TArray<double> Sorted = First.Distances;
		Sorted.Sort();
		const double Mean = MeanOf(First.Distances);
		const FString Report = FString::Printf(TEXT("[Measure] test=T-STRK-003 hits=%d mean=%.4f p90=%.4f max=%.4f horizontal=%.4f vertical=%.4f misses=%d balls=%d no_candidate=%d not_groundstroke=%d out_of_height=%d seed=%d"),
			CountedHits, Mean, Sorted[FMath::FloorToInt32(0.9 * (CountedHits - 1))], Sorted.Last(), First.HorizontalSum / CountedHits,
			First.VerticalSum / CountedHits, First.Misses, First.Balls, First.NoCandidate, First.NotGroundstroke, First.OutOfHeight, Seed);
		const FString TimeReport = FString::Printf(TEXT("[Measure] test=T-STRK-004 press=immediate hits=%d time_error_mean=%.3e time_error_max=%.3e in_range=%d delayed=%d shortened=%d seed=%d"),
			First.TimeErrors.Num(), MeanOf(First.TimeErrors), MaxOf(First.TimeErrors), First.InRangeStarts, First.DelayedStarts, First.ShortenedStarts, Seed);
		for (const FString* Line : { &Report, &TimeReport })
		{
			UE_LOG(LogTennisStroke, Display, TEXT("%s"), **Line);
			TestRunner->AddInfo(*Line);
		}
		ASSERT_THAT(IsTrue(Mean < MaxMeanDistance, *Report));
		ASSERT_THAT(IsTrue(MaxOf(First.TimeErrors) < MaxTimeError, *TimeReport));  // T-STRK-004 の (1)

		// T-STRK-004 の (2)：押す時刻を遅らせて、3 つの始め方を通す（distance_mean は参考の報告だけ。T-STRK-003 には入れない）
		const FMeasure Late = Run(EPress::Late, false);
		const FString LateReport = FString::Printf(TEXT("[Measure] test=T-STRK-004 press=late hits=%d balls=%d no_candidate=%d not_groundstroke=%d out_of_height=%d in_range=%d delayed=%d shortened=%d time_error_mean=%.3e time_error_max=%.3e distance_mean=%.4f seed=%d"),
			Late.Distances.Num(), Late.Balls, Late.NoCandidate, Late.NotGroundstroke, Late.OutOfHeight, Late.InRangeStarts, Late.DelayedStarts,
			Late.ShortenedStarts, MeanOf(Late.TimeErrors), MaxOf(Late.TimeErrors), MeanOf(Late.Distances), Seed);
		UE_LOG(LogTennisStroke, Display, TEXT("%s"), *LateReport);
		TestRunner->AddInfo(LateReport);
		ASSERT_THAT(IsTrue(Late.Failure.IsEmpty(), *Late.Failure));
		ASSERT_THAT(IsTrue(IsDone(Late, EPress::Late), *LateReport));
		ASSERT_THAT(IsTrue(MaxOf(Late.TimeErrors) < MaxTimeError, *LateReport));

		// 同じシードなら、ビットまで同じ（DD-03 §12.1・§12.2、ARCH-03 の決定論）
		const FMeasure Second = Run(EPress::Immediately, false);
		ASSERT_THAT(IsTrue(SameResult(First, Second), TEXT("the same seed gave a different result")));
		const FMeasure LateSecond = Run(EPress::Late, false);
		ASSERT_THAT(IsTrue(SameResult(Late, LateSecond), TEXT("the same seed gave a different result (late presses)")));
	}
};
