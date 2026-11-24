#include "TennisLocomotionDatabaseBuilder.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "TennisGameLog.h"
#include "UObject/Package.h"

namespace
{
	/** 新しいアセットのパッケージ。名前が正しくない・もうあるなら null（警告を出す）。読み取り専用のルート（テストの /Temp）も受け付ける */
	UPackage* NewAssetPackage(const FString& PackageName, FName& OutAssetName)
	{
		FText Reason;
		if (!FPackageName::IsValidLongPackageName(PackageName, true, &Reason))
		{
			UE_LOG(LogTennisMove, Warning, TEXT("[LocomotionBuilder] create package=%s result=invalid reason=%s"), *PackageName, *Reason.ToString());
			return nullptr;
		}
		if (FindPackage(nullptr, *PackageName) != nullptr || FPackageName::DoesPackageExist(PackageName))
		{
			UE_LOG(LogTennisMove, Warning, TEXT("[LocomotionBuilder] create package=%s result=exists"), *PackageName);
			return nullptr;
		}
		OutAssetName = FName(FPackageName::GetShortName(PackageName));
		return CreatePackage(*PackageName);
	}

	void RegisterNewAsset(UObject* Asset)
	{
		FAssetRegistryModule::AssetCreated(Asset);
		Asset->MarkPackageDirty();
	}
}

UPoseSearchSchema* UTennisLocomotionDatabaseBuilder::CreateSchema(const FString& PackageName, USkeleton* Skeleton)
{
	if (Skeleton == nullptr)
	{
		UE_LOG(LogTennisMove, Warning, TEXT("[LocomotionBuilder] create package=%s result=no_skeleton"), *PackageName);
		return nullptr;
	}
	FName AssetName;
	UPackage* Package = NewAssetPackage(PackageName, AssetName);
	if (Package == nullptr)
	{
		return nullptr;
	}
	UPoseSearchSchema* Schema = NewObject<UPoseSearchSchema>(Package, AssetName, RF_Public | RF_Standalone | RF_Transactional);
	Schema->AddSkeleton(Skeleton);
	Schema->AddDefaultChannels();  // 軌跡（−0.4・0・0.35・0.7 s）と両足（位置・速度）：エディタの「新しいスキーマ」と同じ（DATA-02 §4.4）
	RegisterNewAsset(Schema);
	UE_LOG(LogTennisMove, Log, TEXT("[LocomotionBuilder] create schema=%s skeleton=%s result=created"), *PackageName, *Skeleton->GetName());
	return Schema;
}

UPoseSearchDatabase* UTennisLocomotionDatabaseBuilder::CreateDatabase(const FString& PackageName, UPoseSearchSchema* Schema)
{
	if (Schema == nullptr)
	{
		UE_LOG(LogTennisMove, Warning, TEXT("[LocomotionBuilder] create package=%s result=no_schema"), *PackageName);
		return nullptr;
	}
	FName AssetName;
	UPackage* Package = NewAssetPackage(PackageName, AssetName);
	if (Package == nullptr)
	{
		return nullptr;
	}
	UPoseSearchDatabase* Database = NewObject<UPoseSearchDatabase>(Package, AssetName, RF_Public | RF_Standalone | RF_Transactional);
	Database->Schema = Schema;
	RegisterNewAsset(Database);
	UE_LOG(LogTennisMove, Log, TEXT("[LocomotionBuilder] create database=%s schema=%s result=created"), *PackageName, *Schema->GetName());
	return Database;
}

int32 UTennisLocomotionDatabaseBuilder::SetAnimations(UPoseSearchDatabase* Database, const TArray<UAnimSequence*>& Sequences)
{
	const UPoseSearchSchema* Schema = Database != nullptr ? Database->Schema.Get() : nullptr;
	if (Schema == nullptr)
	{
		UE_LOG(LogTennisMove, Warning, TEXT("[LocomotionBuilder] set_animations database=%s result=no_database_or_schema"), *GetNameSafe(Database));
		return -1;
	}
	const USkeleton* Skeleton = Schema->GetSkeleton(Schema->GetDefaultRole());
	Database->Modify();
	while (Database->GetNumAnimationAssets() > 0)
	{
		Database->RemoveAnimationAssetAt(Database->GetNumAnimationAssets() - 1);
	}
	int32 Added = 0;
	for (UAnimSequence* Sequence : Sequences)
	{
		if (Sequence == nullptr || Sequence->GetSkeleton() != Skeleton)
		{
			UE_LOG(LogTennisMove, Warning, TEXT("[LocomotionBuilder] skipped animation=%s skeleton=%s schema_skeleton=%s"),
				*GetNameSafe(Sequence), Sequence != nullptr ? *GetNameSafe(Sequence->GetSkeleton()) : TEXT("none"), *GetNameSafe(Skeleton));
			continue;
		}
		FPoseSearchDatabaseAnimationAsset Entry;
		Entry.AnimAsset = Sequence;
		Database->AddAnimationAsset(Entry);
		++Added;
	}
	Database->PostEditChange();  // 索引を作り直す
	Database->MarkPackageDirty();
	UE_LOG(LogTennisMove, Log, TEXT("[LocomotionBuilder] set_animations database=%s added=%d requested=%d"), *Database->GetName(), Added, Sequences.Num());
	return Added;
}
