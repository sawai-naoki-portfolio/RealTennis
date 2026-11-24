// W-3.5、T-STRK-010：選手のアニメーションの Blueprint（ABP_TennisPlayer）の Slot でストロークのモンタージュを再生すると、
// インパクトの時刻の姿勢で測ったラケットのスイートスポットが、選手の見た目のデータの焼いた値と合う（DD-03 §5.4・§6.2・§7）。
// 焼く道具とは別の経路（実行時のアニメの評価）で、焼いた値を確かめる。
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Chooser.h"
#include "Components/ActorTestSpawner.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/TennisPlayerSettings.h"
#include "Court/TennisCourt.h"
#include "CQTest.h"
#include "Data/StrokeData.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisPlayerAppearance.h"
#include "Player/TennisRacketComponent.h"
#include "Player/TennisStrokeSelection.h"

TEST_CLASS(TennisStrokeMontage, "Tennis.Game.Stroke")
{
	FActorTestSpawner Spawner;

	// DD-03 §6.6：ストロークは、止めたモンタージュの位置をステッパの位置に合わせる（Montage_Pause + Montage_SetPosition）。止めたままでも、
	// Slot の重みはモンタージュの重ね始めの時間で重なっていき、位置はアニメの更新で進まない（置いた位置のまま）
	TEST_METHOD(PausedMontage_BlendsInAndStaysAtTheSetPosition)
	{
		const UChooserTable* Table = GetDefault<UTennisPlayerSettings>()->StrokeChooser.LoadSynchronous();
		FTennisStrokeChooserInput Input;
		Input.Side = EStrokeSide::Forehand;
		const UStrokeData* Stroke = TennisStrokeSelection::Select(Table, Input);
		ASSERT_THAT(IsTrue(Stroke != nullptr && Stroke->Montage != nullptr));
		UAnimMontage* Montage = Stroke->Montage;

		ATennisCharacter& Character = Spawner.SpawnActor<ATennisCharacter>();
		USkeletalMeshComponent* Mesh = Character.GetMesh();
		UAnimInstance* Anim = Mesh->GetAnimInstance();
		ASSERT_THAT(IsTrue(Anim != nullptr && Anim->Montage_Play(Montage, 1.0f) > 0.0f));
		Anim->Montage_Pause(Montage);
		Anim->Montage_SetPosition(Montage, 0.1f);

		const float BlendTime = Montage->GetDefaultBlendInTime();
		const float Step = BlendTime > 0.0f ? BlendTime / 4.0f : 0.05f;
		float Previous = 0.0f;
		for (int32 Tick = 0; Tick < 8; ++Tick)  // 重ね始めの時間の 2 倍
		{
			Mesh->TickAnimation(Step, false);
			Mesh->RefreshBoneTransforms();  // アニメのグラフ（Slot の重み）を評価する
			const FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage);
			const float InstanceWeight = Instance != nullptr ? Instance->GetWeight() : -1.0f;
			const float Weight = Anim->GetSlotMontageGlobalWeight(TEXT("DefaultSlot"));
			const FString State = FString::Printf(TEXT("tick %d: slot %.3f (before %.3f), instance %.3f, blend %.3f s"), Tick, Weight, Previous, InstanceWeight, BlendTime);
			ASSERT_THAT(IsTrue(Weight >= Previous - 1e-4f, *State));
			if (Tick == 0 && BlendTime > 0.0f)
			{
				ASSERT_THAT(IsTrue(Weight > 0.0f && Weight < 1.0f, *State));
			}
			Previous = Weight;
		}
		ASSERT_THAT(IsNear(1.0f, Previous, 1e-3f));
		ASSERT_THAT(IsNear(0.1f, Anim->Montage_GetPosition(Montage), 1e-5f));  // 止めているので進まない

		Anim->Montage_SetPosition(Montage, 0.3f);
		Mesh->TickAnimation(Step, false);
		ASSERT_THAT(IsNear(0.3f, Anim->Montage_GetPosition(Montage), 1e-5f));
		ASSERT_THAT(IsTrue(Anim->Montage_IsPlaying(Montage) == false && Anim->Montage_IsActive(Montage)));
	}

	TEST_METHOD(SlotPlaysTheStroke_SweetSpotMatchesTheBakedOffsetAtImpact)
	{
		const UChooserTable* Table = GetDefault<UTennisPlayerSettings>()->StrokeChooser.LoadSynchronous();
		ASSERT_THAT(IsTrue(Table != nullptr, TEXT("Project Settings > Tennis Player > StrokeChooser")));
		for (const EStrokeSide Side : { EStrokeSide::Forehand, EStrokeSide::Backhand })
		{
			FTennisStrokeChooserInput Input;
			Input.Side = Side;
			const UStrokeData* Stroke = TennisStrokeSelection::Select(Table, Input);
			const TCHAR* SideName = Side == EStrokeSide::Forehand ? TEXT("forehand") : TEXT("backhand");
			ASSERT_THAT(IsTrue(Stroke != nullptr && Stroke->Montage != nullptr, SideName));

			ATennisCharacter& Character = Spawner.SpawnActor<ATennisCharacter>();
			USkeletalMeshComponent* Mesh = Character.GetMesh();
			UAnimInstance* Anim = Mesh->GetAnimInstance();
			ASSERT_THAT(IsTrue(Anim != nullptr));
			FMontageBlendSettings BlendIn;
			BlendIn.Blend.BlendTime = 0.0f;  // 重ね始めの時間を 0 にして、インパクトの姿勢だけを見る
			ASSERT_THAT(IsTrue(Anim->Montage_PlayWithBlendSettings(Stroke->Montage, BlendIn) > 0.0f));
			Anim->Montage_SetPosition(Stroke->Montage, static_cast<float>(Stroke->ImpactTime));
			Anim->Montage_Pause(Stroke->Montage);
			Mesh->TickAnimation(0.0f, false);
			Mesh->RefreshBoneTransforms();
			Mesh->UpdateChildTransforms();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Anim->GetSlotMontageGlobalWeight(TEXT("DefaultSlot")), 1.0f, 1e-3f),
				TEXT("the ABP has no DefaultSlot or the montage is not blended in")));

			const FVector Feet = Character.GetActorLocation() - FVector(0.0, 0.0, ATennisCharacter::CapsuleHalfHeight);
			const FVector Measured = ATennisCourt::WorldToLocalMetersVector(FTransform::Identity,
				Character.GetActorRotation().UnrotateVector(Character.GetRacket()->GetSweetSpotWorld() - Feet));
			const FTennisStrokeFit* Fit = Character.GetAppearance() != nullptr ? Character.GetAppearance()->FindStrokeFit(Stroke) : nullptr;
			ASSERT_THAT(IsTrue(Fit != nullptr, TEXT("the player appearance is not baked (run bake_player_appearances)")));
			const FVector Baked = Fit->SweetSpotOffset.GetLocation();
			ASSERT_THAT(IsTrue(FVector::Dist(Measured, Baked) <= 0.01, *FString::Printf(TEXT("%s: measured %s, baked %s"),  // 1 cm
				*Stroke->GetName(), *Measured.ToString(), *Baked.ToString())));
		}
	}
};
