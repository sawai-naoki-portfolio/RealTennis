// W-3.6：ロコモーションの Motion Matching のスキーマとデータベースを作る道具 UTennisLocomotionDatabaseBuilder（DD-03 §6.5）。
// スキーマは骨格と PoseSearch の既定のチャンネル（軌跡と両足）、データベースはスキーマと、渡したアニメーション（同じ骨格のものだけ）。
// 作る道具はエディタ専用なので、エディタのビルドだけでテストする。アセットは /Temp の下に作り、保存しない。
#include "CQTest.h"

#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Misc/Guid.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchFeatureChannel_Pose.h"
#include "PoseSearch/PoseSearchFeatureChannel_Trajectory.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "TennisLocomotionDatabaseBuilder.h"

namespace
{
	const TCHAR* const SkeletonPath = TEXT("/Game/Tennis/Characters/Base/SK_TennisPlayer_Base.SK_TennisPlayer_Base");
	const TCHAR* const IdlePath = TEXT("/Game/Tennis/Characters/Animations/Locomotion/Mannequin/MM_Idle.MM_Idle");
	const TCHAR* const JogPath = TEXT("/Game/Tennis/Characters/Animations/Locomotion/Mannequin/Jog/MF_Unarmed_Jog_Fwd.MF_Unarmed_Jog_Fwd");

	/** 実行ごとに違う /Temp のパッケージ名（同じエディタで何度走らせても「もうある」にならない） */
	FString TempPackage(const TCHAR* Name)
	{
		return FString::Printf(TEXT("/Temp/TennisTests/%s_%s"), Name, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}
}

TEST_CLASS(TennisLocomotionDatabaseBuilder, "Tennis.Game.Anim")
{
	USkeleton* Skeleton = nullptr;
	TArray<UObject*> Created;  // 作ったアセット（テストの後で片付ける：保存を求められず、残らない）

	AFTER_EACH()
	{
		for (UObject* Asset : Created)
		{
			if (Asset != nullptr)
			{
				Asset->GetPackage()->SetDirtyFlag(false);
				Asset->ClearFlags(RF_Standalone | RF_Public);
				Asset->MarkAsGarbage();
			}
		}
		Created.Reset();
	}

	template <typename TAsset>
	TAsset* Keep(TAsset* Asset)
	{
		Created.Add(Asset);
		return Asset;
	}

	BEFORE_EACH()
	{
		Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath);
		ASSERT_THAT(IsTrue(Skeleton != nullptr));
	}

	// スキーマ：骨格と、既定のチャンネル（軌跡の標本 −0.4・0・0.35・0.7 s、両足の位置と速度：DATA-02 §4.4）。同じ名前は 2 度作らない
	TEST_METHOD(CreateSchema_UsesTheSkeletonAndTheDefaultChannels)
	{
		const FString Package = TempPackage(TEXT("PSS_Test"));
		UPoseSearchSchema* Schema = Keep(UTennisLocomotionDatabaseBuilder::CreateSchema(Package, Skeleton));
		ASSERT_THAT(IsTrue(Schema != nullptr));
		ASSERT_THAT(IsTrue(Schema->GetSkeleton(Schema->GetDefaultRole()) == Skeleton));
		const UPoseSearchFeatureChannel_Trajectory* Trajectory = Schema->FindFirstChannelOfType<UPoseSearchFeatureChannel_Trajectory>();
		ASSERT_THAT(IsTrue(Trajectory != nullptr));
		TArray<float> Offsets;
		for (const FPoseSearchTrajectorySample& Sample : Trajectory->Samples)
		{
			Offsets.Add(Sample.Offset);
		}
		ASSERT_THAT(IsTrue(Offsets == TArray<float>({ -0.4f, 0.0f, 0.35f, 0.7f }), *FString::JoinBy(Offsets, TEXT(","), [](float V) { return FString::SanitizeFloat(V); })));
		const UPoseSearchFeatureChannel_Pose* Pose = Schema->FindFirstChannelOfType<UPoseSearchFeatureChannel_Pose>();
		ASSERT_THAT(IsTrue(Pose != nullptr && Pose->SampledBones.Num() == 2));
		ASSERT_THAT(IsTrue(Pose->SampledBones[0].Reference.BoneName == TEXT("foot_l") && Pose->SampledBones[1].Reference.BoneName == TEXT("foot_r")));

		ASSERT_THAT(IsTrue(UTennisLocomotionDatabaseBuilder::CreateSchema(Package, Skeleton) == nullptr));  // もうある
		ASSERT_THAT(IsTrue(UTennisLocomotionDatabaseBuilder::CreateSchema(TempPackage(TEXT("PSS_NoSkeleton")), nullptr) == nullptr));
		ASSERT_THAT(IsTrue(UTennisLocomotionDatabaseBuilder::CreateSchema(TEXT("not a package"), Skeleton) == nullptr));
	}

	// データベース：スキーマと、渡したアニメーション（この順。null は飛ばす）。入れ直すと前のものは残らない
	TEST_METHOD(SetAnimations_ReplacesTheAnimations)
	{
		UPoseSearchSchema* Schema = Keep(UTennisLocomotionDatabaseBuilder::CreateSchema(TempPackage(TEXT("PSS_Test")), Skeleton));
		UPoseSearchDatabase* Database = Keep(UTennisLocomotionDatabaseBuilder::CreateDatabase(TempPackage(TEXT("PSD_Test")), Schema));
		ASSERT_THAT(IsTrue(Database != nullptr && Database->Schema == Schema));
		UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, IdlePath);
		UAnimSequence* Jog = LoadObject<UAnimSequence>(nullptr, JogPath);
		ASSERT_THAT(IsTrue(Idle != nullptr && Jog != nullptr));

		// 骨格の違うアニメーションは飛ばす（一時の骨格と、それを使うアニメーション）
		USkeleton* OtherSkeleton = NewObject<USkeleton>(GetTransientPackage());
		UAnimSequence* Other = NewObject<UAnimSequence>(GetTransientPackage());
		Other->SetSkeleton(OtherSkeleton);
		ASSERT_THAT(AreEqual(2, UTennisLocomotionDatabaseBuilder::SetAnimations(Database, { Idle, nullptr, Other, Jog })));
		ASSERT_THAT(AreEqual(2, Database->GetNumAnimationAssets()));
		ASSERT_THAT(IsTrue(Database->GetAnimationAsset(0) == Idle && Database->GetAnimationAsset(1) == Jog));
		ASSERT_THAT(AreEqual(1, UTennisLocomotionDatabaseBuilder::SetAnimations(Database, { Jog })));
		ASSERT_THAT(AreEqual(1, Database->GetNumAnimationAssets()));
		ASSERT_THAT(IsTrue(Database->GetAnimationAsset(0) == Jog));

		ASSERT_THAT(AreEqual(-1, UTennisLocomotionDatabaseBuilder::SetAnimations(nullptr, { Jog })));
		ASSERT_THAT(IsTrue(UTennisLocomotionDatabaseBuilder::CreateDatabase(TempPackage(TEXT("PSD_NoSchema")), nullptr) == nullptr));
	}
};
#endif
