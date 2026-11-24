// W-3.6、T-MOVE-008：選手のアニメーションのインスタンス UTennisAnimInstance（DD-03 §6.5）。Motion Matching に渡す軌跡は、過去（メッシュの
// 位置の履歴）・今・未来（UTennisMovementComponent::PredictWorldPath ＝ 移動のモデルの予測）で、時刻が並び、未来は自動移動の目標へ向かう。
// 振っていない間の下半身の重み（T-STRK-018 のつなぎこみ）：モンタージュがなければ 1、動いていれば 0、ブレンドアウト中は前の値のまま。
// 置いた ABP とデータベース。アニメの更新とメッシュ以外の Tick は、このテストが手で呼ぶ。
#include "Components/ActorTestSpawner.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Court/TennisCourt.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Core/TennisPlayerSettings.h"
#include "CQTest.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/WorldSettings.h"
#include "Player/TennisAnimInstance.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisMovementComponent.h"
#include "TennisGameTestHelpers.h"

#if WITH_EDITOR
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#endif

namespace
{
	constexpr float AnimFrame = 1.0f / 50.0f;  // s（240 Hz で割り切れない：まだステップにしていない時間が残る）
}

TEST_CLASS(TennisAnimInstance, "Tennis.Game.Anim")
{
	FActorTestSpawner Spawner;
	ATennisCourt* Court = nullptr;
	ATennisCharacter* Character = nullptr;
	UTennisAnimInstance* Anim = nullptr;
	float SavedTimeScale = 1.0f;

	BEFORE_EACH()
	{
		SavedTimeScale = GetDefault<UTennisDebugSettings>()->GetTimeScale();
		TennisTest::SetTimeScale(1.0f);
		Court = &Spawner.SpawnActor<ATennisCourt>();  // 原点（コート局所 = ワールド。1 m = 100 cm）
		AStaticMeshActor& Floor = Spawner.SpawnActorAt<AStaticMeshActor>(FVector(0.0, 0.0, -50.0), FRotator::ZeroRotator);
		Floor.GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Floor.GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Floor.SetActorScale3D(FVector(60.0, 30.0, 1.0));
		Floor.GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		Character = &Spawner.SpawnActorAt<ATennisCharacter>(Court->LocalMetersToWorld(FVector(-12.0, 0.0, 1.0)), FRotator::ZeroRotator);
		Spawner.GetWorld().GetWorldSettings()->NotifyBeginPlay();
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		Movement->bRunPhysicsWithNoController = true;  // 操作する人なしで動かす
		Movement->SetMovementMode(MOVE_Walking);
		Character->GetMesh()->SetAnimInstanceClass(UTennisAnimInstance::StaticClass());  // Blueprint の絵（グラフ）なしで、親のクラスだけ
		Anim = Cast<UTennisAnimInstance>(Character->GetMesh()->GetAnimInstance());
	}

	AFTER_EACH()
	{
		Spawner.GetWorld().EndPlay(EEndPlayReason::Quit);
		TennisTest::SetTimeScale(SavedTimeScale);
	}

	/** 1 フレーム：移動 → アニメの更新 */
	void TickFrame()
	{
		Character->GetCharacterMovement()->TickComponent(AnimFrame, LEVELTICK_All, nullptr);
		Anim->UpdateAnimation(AnimFrame, false);
		Anim->DispatchQueuedAnimEvents();  // メッシュの通常の更新と同じ順に終了イベントを配送する
	}

	// T-MOVE-008：過去 4・今 1・未来 8 の標本が −0.4〜0.8 s に並び、過去は記録したメッシュの位置、今はメッシュの位置、未来はモデルの予測
	TEST_METHOD(Trajectory_HasThePastTheNowAndThePrediction)
	{
		ASSERT_THAT(IsTrue(Anim != nullptr));
		const FVector Target = Court->LocalMetersToWorld(FVector(-10.0, 2.5, 0.0));
		Character->GetTennisMovement()->SetAutoMoveTarget(FVector2D(-10.0, 2.5), 1.5, 0.2);
		TArray<FVector> MeshPositions;
		for (int32 Frame = 0; Frame < 30; ++Frame)
		{
			TickFrame();
			MeshPositions.Add(Character->GetMesh()->GetComponentLocation());
		}
		const TArray<FTransformTrajectorySample>& Samples = Anim->Trajectory.Samples;
		ASSERT_THAT(AreEqual(13, Samples.Num()));
		for (int32 Index = 0; Index < Samples.Num(); ++Index)
		{
			ASSERT_THAT(IsNear(0.1f * (Index - 4), Samples[Index].TimeInSeconds, 1e-5f));
		}
		const auto Show = [](const TCHAR* What, const FVector& A, const FVector& B) { return FString::Printf(TEXT("%s: %s vs %s"), What, *A.ToString(), *B.ToString()); };
		ASSERT_THAT(IsTrue(Samples[4].Position.Equals(MeshPositions.Last(), 1e-6), *Show(TEXT("now"), Samples[4].Position, MeshPositions.Last())));
		const FVector Ago04 = MeshPositions[MeshPositions.Num() - 21];  // 0.4 s 前 = 20 フレーム前
		const FVector Ago01 = MeshPositions[MeshPositions.Num() - 6];   // 0.1 s 前 = 5 フレーム前
		ASSERT_THAT(IsTrue(Samples[0].Position.Equals(Ago04, 0.01), *Show(TEXT("-0.4 s"), Samples[0].Position, Ago04)));
		ASSERT_THAT(IsTrue(Samples[3].Position.Equals(Ago01, 0.01), *Show(TEXT("-0.1 s"), Samples[3].Position, Ago01)));

		const double Times[] = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8 };
		const TArray<FVector> Path = Character->GetTennisMovement()->PredictWorldPath(Times);
		const FVector Offset = Character->GetMesh()->GetComponentLocation() - Character->GetActorLocation();
		ASSERT_THAT(AreEqual(8, Path.Num()));
		for (int32 Index = 0; Index < 8; ++Index)  // 時刻は float の間隔から作るので、1e-8 s ほどずれる（0.01 cm 以内）
		{
			ASSERT_THAT(IsTrue(Samples[5 + Index].Position.Equals(Path[Index] + Offset, 0.01), *Show(TEXT("future"), Samples[5 + Index].Position, Path[Index] + Offset)));
		}
		const auto Planar = [](const FVector& A, const FVector& B) { return FVector2D::Distance(FVector2D(A), FVector2D(B)); };
		ASSERT_THAT(IsTrue(Planar(Samples.Last().Position, Target) < Planar(Samples[4].Position, Target) - 50.0,
			*FString::Printf(TEXT("toward the target: %.1f -> %.1f cm"), Planar(Samples[4].Position, Target), Planar(Samples.Last().Position, Target))));
		ASSERT_THAT(IsTrue(Samples[4].Facing.Equals(Character->GetMesh()->GetComponentQuat(), 1e-6), TEXT("facing")));
	}

	// T-MOVE-008：加速の途中で予測した位置（今・0.1 s・0.2 s 後）が、同じ入力（自動移動）でその時間だけ動いた後のカプセルの位置と合う。
	// 最後のステップの位置（外の位置 − Lead）と、まだステップにしていない時間（Remainder）の扱いを確かめる（どちらかを落とすと 0.1 cm 以上ずれる）
	TEST_METHOD(PredictWorldPath_MatchesWhereTheMovementGoes)
	{
		Character->GetTennisMovement()->SetAutoMoveTarget(FVector2D(-10.0, 2.5), 1.5, 0.2);
		for (int32 Frame = 0; Frame < 7; ++Frame)
		{
			TickFrame();
		}
		const double Times[] = { 0.0, 0.1, 0.2 };
		const TArray<FVector> Path = Character->GetTennisMovement()->PredictWorldPath(Times);
		ASSERT_THAT(AreEqual(3, Path.Num()));
		const auto Gap = [this](const FVector& Predicted) { return FVector2D::Distance(FVector2D(Predicted), FVector2D(Character->GetActorLocation())); };
		ASSERT_THAT(IsTrue(Gap(Path[0]) < 0.05, *FString::Printf(TEXT("t=0: %.4f cm"), Gap(Path[0]))));
		for (int32 Frame = 0; Frame < 5; ++Frame)  // 0.1 s
		{
			TickFrame();
		}
		ASSERT_THAT(IsTrue(Gap(Path[1]) < 0.05, *FString::Printf(TEXT("t=0.1: %.4f cm"), Gap(Path[1]))));
		for (int32 Frame = 0; Frame < 5; ++Frame)
		{
			TickFrame();
		}
		ASSERT_THAT(IsTrue(Gap(Path[2]) < 0.05, *FString::Printf(TEXT("t=0.2: %.4f cm"), Gap(Path[2]))));
	}

	UAnimMontage* LoadStrokeMontage()
	{
		return LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Tennis/Characters/Animations/Strokes/AM_FH_Mid_Drive_Square.AM_FH_Mid_Drive_Square"));
	}

	// T-STRK-018（つなぎこみ）：ストロークの外でモンタージュが動いている間は全身のモンタージュ（0）。ブレンドアウトの間は 0 のまま、
	// 終わると 1 に戻る（腰と脚が急に替わらない）
	TEST_METHOD(LowerBodyAlpha_IsZeroWhileAMontagePlaysOutsideAStroke)
	{
		UAnimMontage* Montage = LoadStrokeMontage();
		ASSERT_THAT(IsTrue(Montage != nullptr));
		ASSERT_THAT(IsTrue(Anim->Montage_Play(Montage) > 0.0f));
		TickFrame();
		ASSERT_THAT(IsNear(0.0f, Anim->LowerBodyLocomotionAlpha, 1e-6f));
		Anim->Montage_Stop(0.25f, Montage);  // 0.25 s かけて消える
		TickFrame();
		ASSERT_THAT(IsTrue(Anim->IsAnyMontagePlaying()));
		ASSERT_THAT(IsNear(0.0f, Anim->LowerBodyLocomotionAlpha, 1e-6f));  // ブレンドアウトの間は前の値
		for (int32 Frame = 0; Frame < 20; ++Frame)  // 0.4 s
		{
			TickFrame();
		}
		ASSERT_THAT(IsFalse(Anim->IsAnyMontagePlaying()));
		ASSERT_THAT(IsNear(1.0f, Anim->LowerBodyLocomotionAlpha, 1e-6f));
	}

	// T-STRK-018（つなぎこみ）：重みが 1 のときに始まったブレンドアウト（テイクバックの間の取り消し）は、1 のまま（脚がモンタージュへ戻らない）
	TEST_METHOD(LowerBodyAlpha_KeepsOneThroughABlendOutThatStartsAtOne)
	{
		UAnimMontage* Montage = LoadStrokeMontage();
		ASSERT_THAT(IsTrue(Montage != nullptr));
		TickFrame();
		ASSERT_THAT(IsNear(1.0f, Anim->LowerBodyLocomotionAlpha, 1e-6f));
		ASSERT_THAT(IsTrue(Anim->Montage_Play(Montage) > 0.0f));
		TickFrame();  // 停止の前にモンタージュの重みを立ち上げる
		Anim->LowerBodyLocomotionAlpha = 1.0f;  // テイクバックを取り消した時点の初期値
		Anim->Montage_Stop(0.25f, Montage);
		TickFrame();
		ASSERT_THAT(IsTrue(Anim->IsAnyMontagePlaying()));
		ASSERT_THAT(IsNear(1.0f, Anim->LowerBodyLocomotionAlpha, 1e-6f));
	}

	// 置いたアセット（DD-03 §6.5）：選手の ABP は UTennisAnimInstance の子。データベース PSD_Locomotion は、骨格 SK_TennisPlayer_Base の
	// スキーマで、M3 の仮のデータ 17 本（待機と、歩き・ジョグの 8 方向ずつ）を持つ
	TEST_METHOD(PlacedAssets_UseTheAnimInstanceAndTheLocomotionDatabase)
	{
		const UClass* AnimClass = GetDefault<UTennisPlayerSettings>()->AnimClass.LoadSynchronous();
		ASSERT_THAT(IsTrue(AnimClass != nullptr && AnimClass->IsChildOf(UTennisAnimInstance::StaticClass()), *GetNameSafe(AnimClass)));
#if WITH_EDITOR
		const UPoseSearchDatabase* Database = LoadObject<UPoseSearchDatabase>(nullptr,
			TEXT("/Game/Tennis/Characters/Animations/Locomotion/PSD_Locomotion.PSD_Locomotion"));
		ASSERT_THAT(IsTrue(Database != nullptr && Database->Schema != nullptr));
		ASSERT_THAT(AreEqual(FString(TEXT("SK_TennisPlayer_Base")), GetNameSafe(Database->Schema->GetSkeleton(Database->Schema->GetDefaultRole()))));
		ASSERT_THAT(AreEqual(17, Database->GetNumAnimationAssets()));
#endif
	}

	// 振っていない間は、下半身はすべて Motion Matching
	TEST_METHOD(LowerBodyAlpha_IsOneWhileNotStroking)
	{
		Anim->LowerBodyLocomotionAlpha = 0.0f;  // 更新が書くことを確かめる
		TickFrame();
		ASSERT_THAT(IsNear(1.0f, Anim->LowerBodyLocomotionAlpha, 1e-6f));
	}
};
