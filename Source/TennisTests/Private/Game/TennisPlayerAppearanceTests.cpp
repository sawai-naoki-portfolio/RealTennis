// 選手の見た目（DD-03 §1・§7、ART-01 §3。T-CHR-001）：見た目のデータ（体と追従する部品と髪）、ラケットの付け方（grip を手のソケットに重ねる）、スイートスポット。
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Components/ActorTestSpawner.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/TennisPlayerSettings.h"
#include "CQTest.h"
#include "Data/StrokeData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "GroomComponent.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisPlayerAppearance.h"
#include "Player/TennisRacketComponent.h"
#include "Player/TennisReachComponent.h"

TEST_CLASS(TennisPlayerAppearance, "Tennis.Game.Player")
{
	FActorTestSpawner Spawner;

	// 設定は、見た目のデータ・SK_TennisPlayer_Base のアニメーションの Blueprint・grip と racket_sweetspot を持つラケットを指す。見た目の
	// データの体の骨格は SK_TennisPlayer_Base か、それを互換の骨格に持つ（既定は MetaHuman の M01：利用者の判断、2026-11-18）
	TEST_METHOD(Settings_PointToThePlayerAnimationAndRacket)
	{
		const UTennisPlayerSettings* Settings = GetDefault<UTennisPlayerSettings>();
		const UTennisPlayerAppearance* Appearance = Settings->Appearance.LoadSynchronous();
		ASSERT_THAT(IsTrue(Appearance != nullptr));
		const USkeletalMesh* Mesh = Appearance->Body.LoadSynchronous();
		const UAnimBlueprintGeneratedClass* AnimClass = Cast<UAnimBlueprintGeneratedClass>(Settings->AnimClass.LoadSynchronous());
		const UStaticMesh* Racket = Settings->RacketMesh.LoadSynchronous();
		ASSERT_THAT(IsTrue(Mesh != nullptr && AnimClass != nullptr && Racket != nullptr));
		const USkeleton* Base = AnimClass->TargetSkeleton;
		ASSERT_THAT(AreEqual(FString(TEXT("SK_TennisPlayer_Base")), GetNameSafe(Base)));
		const USkeleton* Skeleton = Mesh->GetSkeleton();
		ASSERT_THAT(IsTrue(Skeleton == Base || (Skeleton != nullptr && Skeleton->GetCompatibleSkeletons().Contains(TSoftObjectPtr<USkeleton>(Base)))));
		ASSERT_THAT(IsTrue(Mesh->FindSocket(ATennisCharacter::RacketHandSocket) != nullptr));
		ASSERT_THAT(IsTrue(Racket->FindSocket(UTennisRacketComponent::GripSocket) != nullptr));
		ASSERT_THAT(IsTrue(Racket->FindSocket(UTennisRacketComponent::SweetSpotSocket) != nullptr));
	}

	// メッシュの足元はカプセルの底、前はキャラクターの前（+X）。メッシュがあるのでカプセルは隠す
	TEST_METHOD(Character_ShowsThePlayerOnTheCapsuleBottom)
	{
		ATennisCharacter& Character = Spawner.SpawnActor<ATennisCharacter>();
		const USkeletalMeshComponent* Mesh = Character.GetMesh();
		ASSERT_THAT(IsTrue(Mesh->GetSkeletalMeshAsset() == GetDefault<UTennisPlayerSettings>()->Appearance.LoadSynchronous()->Body.LoadSynchronous()));
		ASSERT_THAT(IsTrue(Mesh->GetRelativeLocation().Equals(FVector(0.0, 0.0, -ATennisCharacter::CapsuleHalfHeight), 1e-6)));
		ASSERT_THAT(IsTrue(Mesh->GetRelativeRotation().Equals(FRotator(0.0, -90.0, 0.0), 1e-6)));
		ASSERT_THAT(IsTrue(Character.GetCapsuleComponent()->bHiddenInGame));
		ASSERT_THAT(IsTrue(Mesh->GetAnimInstance() != nullptr
			&& Mesh->GetAnimInstance()->GetClass() == GetDefault<UTennisPlayerSettings>()->AnimClass.LoadSynchronous()));  // ABP_TennisPlayer で動く
	}

	// ラケットは手のソケットに付き、ラケットの grip が手のソケットに重なる（位置と向き）。スイートスポットは grip から 42.1 cm 先
	TEST_METHOD(Racket_GripSitsOnTheHandSocket)
	{
		ATennisCharacter& Character = Spawner.SpawnActor<ATennisCharacter>();
		const UTennisRacketComponent* Racket = Character.GetRacket();
		ASSERT_THAT(IsTrue(Racket->GetStaticMesh() == GetDefault<UTennisPlayerSettings>()->RacketMesh.LoadSynchronous()));
		ASSERT_THAT(IsTrue(Racket->GetAttachParent() == Character.GetMesh()));
		ASSERT_THAT(IsTrue(Racket->GetAttachSocketName() == ATennisCharacter::RacketHandSocket));
		ASSERT_THAT(IsTrue(Racket->GetCollisionEnabled() == ECollisionEnabled::NoCollision));

		const FTransform Hand = Character.GetMesh()->GetSocketTransform(ATennisCharacter::RacketHandSocket);
		const FTransform Grip = Racket->GetSocketTransform(UTennisRacketComponent::GripSocket);
		ASSERT_THAT(IsTrue(Grip.GetLocation().Equals(Hand.GetLocation(), 0.01)));
		ASSERT_THAT(IsTrue(Grip.GetRotation().Equals(Hand.GetRotation(), 1e-4)));

		const FVector SweetSpot = Racket->GetSweetSpotWorld();
		ASSERT_THAT(IsTrue(SweetSpot.Equals(Racket->GetSocketLocation(UTennisRacketComponent::SweetSpotSocket), 1e-6)));
		ASSERT_THAT(IsNear(48.1 - 6.0, FVector::Dist(SweetSpot, Grip.GetLocation()), 0.05));  // ART-01 §2：48.1 cm と 6 cm
		// 先端はラケットの +X：grip から見てスイートスポットは、手のソケットの +X の向き
		ASSERT_THAT(IsTrue(FVector::DotProduct((SweetSpot - Grip.GetLocation()).GetSafeNormal(), Hand.GetUnitAxis(EAxis::X)) > 0.999));
	}

	// 追従する部品（顔・服）は体に付き、体の骨の姿勢を使う（Leader Pose）。メッシュのない部品は飛ばす。付け直すと前の部品は消える
	TEST_METHOD(Followers_UseTheBodyPose)
	{
		ATennisCharacter& Character = Spawner.SpawnActor<ATennisCharacter>();
		const UTennisPlayerAppearance* Default = GetDefault<UTennisPlayerSettings>()->Appearance.LoadSynchronous();
		ASSERT_THAT(IsTrue(Default != nullptr));
		UTennisPlayerAppearance* Appearance = NewObject<UTennisPlayerAppearance>();
		Appearance->Body = Default->Body;
		Appearance->Followers = {Default->Body, TSoftObjectPtr<USkeletalMesh>()};  // 2 つ目はメッシュがない（警告を出して飛ばす）
		ASSERT_THAT(IsTrue(Character.ApplyAppearance(Appearance)));
		ASSERT_THAT(AreEqual(1, Character.GetFollowers().Num()));
		const USkeletalMeshComponent* Follower = Character.GetFollowers()[0];
		ASSERT_THAT(IsTrue(Follower->GetAttachParent() == Character.GetMesh()));
		ASSERT_THAT(IsTrue(Follower->LeaderPoseComponent.Get() == Character.GetMesh()));
		ASSERT_THAT(IsTrue(Follower->IsRegistered()));
		ASSERT_THAT(IsTrue(Follower->GetCollisionEnabled() == ECollisionEnabled::NoCollision));

		Appearance->Followers.Reset();
		ASSERT_THAT(IsTrue(Character.ApplyAppearance(Appearance)));
		ASSERT_THAT(AreEqual(0, Character.GetFollowers().Num()));
	}

	// 髪は Binding の対象のメッシュを持つ部品に付く（追従する部品を先に見る。なければ体）。Binding のない髪・Groom のない Binding は
	// 飛ばす（警告）。付け直すと前の髪は消える（DD-03 §7）
	TEST_METHOD(Grooms_AttachToThePartTheBindingTargets)
	{
		ATennisCharacter& Character = Spawner.SpawnActor<ATennisCharacter>();
		const UTennisPlayerAppearance* Default = GetDefault<UTennisPlayerSettings>()->Appearance.LoadSynchronous();
		ASSERT_THAT(IsTrue(Default != nullptr));
		USkeletalMesh* Part = Default->Body.LoadSynchronous();
		ASSERT_THAT(IsTrue(Part != nullptr));
		UGroomAsset* Groom = NewObject<UGroomAsset>();
		auto MakeBinding = [](USkeletalMesh* Target, UGroomAsset* Asset)
		{
			UGroomBindingAsset* Binding = NewObject<UGroomBindingAsset>();
			Binding->SetGroom(Asset);
			Binding->SetTargetSkeletalMesh(Target);
			return TSoftObjectPtr<UGroomBindingAsset>(Binding);
		};
		UTennisPlayerAppearance* Appearance = NewObject<UTennisPlayerAppearance>();
		Appearance->Body = Default->Body;
		Appearance->Followers = {Default->Body};  // MetaHuman の顔の代わりの部品（体と同じメッシュ：追従する部品を先に見る）
		Appearance->GroomBindings = {
			MakeBinding(Part, Groom),                        // 部品に付く
			MakeBinding(NewObject<USkeletalMesh>(), Groom),  // 対象を持つ部品がない：体に付ける（警告）
			TSoftObjectPtr<UGroomBindingAsset>(),            // Binding がない：飛ばす（警告）
			MakeBinding(Part, nullptr)};                     // Groom がない：飛ばす（警告）
		ASSERT_THAT(IsTrue(Character.ApplyAppearance(Appearance)));
		ASSERT_THAT(AreEqual(1, Character.GetFollowers().Num()));
		ASSERT_THAT(AreEqual(2, Character.GetGrooms().Num()));
		ASSERT_THAT(IsTrue(Character.GetGrooms()[0]->GetAttachParent() == Character.GetFollowers()[0]));
		ASSERT_THAT(IsTrue(Character.GetGrooms()[1]->GetAttachParent() == Character.GetMesh()));
		for (const UGroomComponent* Hair : Character.GetGrooms())
		{
			ASSERT_THAT(IsTrue(Hair->IsRegistered()));
			ASSERT_THAT(IsTrue(Hair->GetCollisionEnabled() == ECollisionEnabled::NoCollision));
		}
		const TWeakObjectPtr<UGroomComponent> Old = Character.GetGrooms()[0];

		// 追従する部品がなければ、対象が体のメッシュの髪（警告なし）も、対象のない髪（警告）も体に付く。前の髪は消える
		Appearance->Followers.Reset();
		Appearance->GroomBindings = {MakeBinding(Part, Groom), MakeBinding(nullptr, Groom)};
		ASSERT_THAT(IsTrue(Character.ApplyAppearance(Appearance)));
		ASSERT_THAT(IsFalse(Old.IsValid()));
		ASSERT_THAT(AreEqual(2, Character.GetGrooms().Num()));
		ASSERT_THAT(IsTrue(Character.GetGrooms()[0]->GetAttachParent() == Character.GetMesh()));
		ASSERT_THAT(IsTrue(Character.GetGrooms()[1]->GetAttachParent() == Character.GetMesh()));

		Appearance->GroomBindings.Reset();
		ASSERT_THAT(IsTrue(Character.ApplyAppearance(Appearance)));
		ASSERT_THAT(AreEqual(0, Character.GetGrooms().Num()));
	}

	// 焼いた値（DD-03 §7）：ストロークごとに引ける。ないストローク・null は nullptr。探索の体の位置は、どれも正の数のときだけ焼いてある
	TEST_METHOD(BakedValues_AreFoundPerStroke)
	{
		UStrokeData* Baked = NewObject<UStrokeData>();
		UStrokeData* NotBaked = NewObject<UStrokeData>();
		UTennisPlayerAppearance* Appearance = NewObject<UTennisPlayerAppearance>();
		Appearance->StrokeFits = {FTennisStrokeFit{TSoftObjectPtr<UStrokeData>(Baked), FTransform(FVector(0.4, 0.8, 1.0))}};
		const FTennisStrokeFit* Fit = Appearance->FindStrokeFit(Baked);
		ASSERT_THAT(IsTrue(Fit != nullptr && Fit->SweetSpotOffset.GetLocation().Equals(FVector(0.4, 0.8, 1.0))));
		ASSERT_THAT(IsTrue(Appearance->FindStrokeFit(NotBaked) == nullptr));
		ASSERT_THAT(IsTrue(Appearance->FindStrokeFit(nullptr) == nullptr));

		FTennisReachBody Reach;
		ASSERT_THAT(IsFalse(Reach.IsBaked()));  // 焼いていない（0）
		Reach.Forward = 0.38;
		Reach.ForehandSide = 0.82;
		Reach.OneHandedBackhandSide = 0.62;
		Reach.TwoHandedBackhandSide = 0.62;
		ASSERT_THAT(IsTrue(Reach.IsBaked()));
		Reach.TwoHandedBackhandSide = std::numeric_limits<double>::quiet_NaN();
		ASSERT_THAT(IsFalse(Reach.IsBaked()));
		Reach.TwoHandedBackhandSide = -0.62;
		ASSERT_THAT(IsFalse(Reach.IsBaked()));
	}

	// 探索の体の位置は、付けた見た目のデータの焼いた値（DD-03 §4.2）。焼いていない・体がなければ、パラメータ表の既定（前の体の値を残さない）。
	// パラメータ表の既定は、仮の男性の見た目のデータの焼いた値と 5 cm 以内で合う（DATA-02 §4.1、T-STRK-011。既定の選手が MetaHuman でも）
	TEST_METHOD(ReachBody_ComesFromTheAppearance)
	{
		ATennisCharacter& Character = Spawner.SpawnActor<ATennisCharacter>();
		const UTennisPlayerAppearance* Default = GetDefault<UTennisPlayerSettings>()->Appearance.LoadSynchronous();
		ASSERT_THAT(IsTrue(Default != nullptr && Default->ReachBody.IsBaked(), TEXT("run bake_player_appearances")));
		const FReachParams& Params = Character.GetReach()->Params;
		auto Matches = [&Params](const FTennisReachBody& Body)
		{
			return Params.Forward == Body.Forward && Params.ForehandSide == Body.ForehandSide
				&& Params.OneHandedBackhandSide == Body.OneHandedBackhandSide && Params.TwoHandedBackhandSide == Body.TwoHandedBackhandSide;
		};
		ASSERT_THAT(IsTrue(Character.GetAppearance() == Default && Matches(Default->ReachBody)));
		const FReachParams Defaults;
		FTennisReachBody DefaultBody;
		DefaultBody.Forward = Defaults.Forward;
		DefaultBody.ForehandSide = Defaults.ForehandSide;
		DefaultBody.OneHandedBackhandSide = Defaults.OneHandedBackhandSide;
		DefaultBody.TwoHandedBackhandSide = Defaults.TwoHandedBackhandSide;
		const UTennisPlayerAppearance* InterimMale = LoadObject<UTennisPlayerAppearance>(nullptr,
			TEXT("/Game/Tennis/Characters/Players/Interim/DA_Appearance_Player_Male01.DA_Appearance_Player_Male01"));
		ASSERT_THAT(IsTrue(InterimMale != nullptr && InterimMale->ReachBody.IsBaked(), TEXT("DA_Appearance_Player_Male01 (DATA-02 §4.1): run bake_player_appearances")));
		const FTennisReachBody& Baked = InterimMale->ReachBody;
		ASSERT_THAT(IsTrue(FMath::Abs(Baked.Forward - Defaults.Forward) <= 0.05 && FMath::Abs(Baked.ForehandSide - Defaults.ForehandSide) <= 0.05
			&& FMath::Abs(Baked.OneHandedBackhandSide - Defaults.OneHandedBackhandSide) <= 0.05
			&& FMath::Abs(Baked.TwoHandedBackhandSide - Defaults.TwoHandedBackhandSide) <= 0.05));

		UTennisPlayerAppearance* Other = NewObject<UTennisPlayerAppearance>();
		Other->Body = Default->Body;
		Other->ReachBody.Forward = 0.30;
		Other->ReachBody.ForehandSide = 0.70;
		Other->ReachBody.OneHandedBackhandSide = 0.60;
		Other->ReachBody.TwoHandedBackhandSide = 0.50;
		ASSERT_THAT(IsTrue(Character.ApplyAppearance(Other)));
		ASSERT_THAT(IsTrue(Character.GetAppearance() == Other && Matches(Other->ReachBody)));

		UTennisPlayerAppearance* NotBaked = NewObject<UTennisPlayerAppearance>();
		NotBaked->Body = Default->Body;
		ASSERT_THAT(IsTrue(Character.ApplyAppearance(NotBaked)));
		ASSERT_THAT(IsTrue(Character.GetAppearance() == NotBaked && Matches(DefaultBody)));
		ASSERT_THAT(IsTrue(Character.ApplyAppearance(Other)));
		ASSERT_THAT(IsFalse(Character.ApplyAppearance(nullptr)));  // 体がない
		ASSERT_THAT(IsTrue(Character.GetAppearance() == nullptr && Matches(DefaultBody)));
	}

	// 体のない見た目は付けない：カプセルを見せ、ラケットを隠して false（警告を出す）
	TEST_METHOD(Appearance_WithoutBodyShowsTheCapsule)
	{
		ATennisCharacter& Character = Spawner.SpawnActor<ATennisCharacter>();
		ASSERT_THAT(IsFalse(Character.ApplyAppearance(nullptr)));
		ASSERT_THAT(IsFalse(Character.GetCapsuleComponent()->bHiddenInGame));
		ASSERT_THAT(IsTrue(Character.GetRacket()->bHiddenInGame));
	}

	// ラケットのメッシュがない・grip がないときは付けない（警告を出し、false）
	TEST_METHOD(Racket_WithoutGripIsRejected)
	{
		UTennisRacketComponent* Racket = NewObject<UTennisRacketComponent>();
		ASSERT_THAT(IsFalse(Racket->SetRacketMesh(nullptr)));
		ASSERT_THAT(IsTrue(Racket->GetStaticMesh() == nullptr));
	}
};
