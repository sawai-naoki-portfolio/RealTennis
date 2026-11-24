// Motion Matching のスキーマ PSS_Locomotion とデータベース PSD_Locomotion を作る・アニメーションを入れ直す（DD-03 §6.5）。
// PoseSearch のファクトリーの設定（骨格・スキーマ）とデータベースのアニメーションの追加は Python から使えないので C++ で行う。
// どのアセットに何を入れるか・保存するのは MCP のツール（Tools/Unreal の TennisTools.build_locomotion_database）が決める。
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TennisLocomotionDatabaseBuilder.generated.h"

class UAnimSequence;
class UPoseSearchDatabase;
class UPoseSearchSchema;
class USkeleton;

UCLASS()
class TENNISEDITOR_API UTennisLocomotionDatabaseBuilder : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * パッケージ PackageName（例 "/Game/Tennis/Characters/Animations/Locomotion/PSS_Locomotion"）に、骨格 Skeleton のスキーマを作る。
	 * チャンネルは PoseSearch の既定（軌跡と両足の位置・速度：エディタの「新しいスキーマ」と同じ。DATA-02 §4.4）。もうアセットがある・
	 * 骨格が null・名前が正しくなければ、作らずに null（警告を出す）。保存はしない
	 */
	UFUNCTION(BlueprintCallable, Category = "Tennis|Editor")
	static UPoseSearchSchema* CreateSchema(const FString& PackageName, USkeleton* Skeleton);

	/** パッケージ PackageName に、スキーマ Schema のデータベースを作る（アニメーションは空）。条件は CreateSchema と同じ。保存はしない */
	UFUNCTION(BlueprintCallable, Category = "Tennis|Editor")
	static UPoseSearchDatabase* CreateDatabase(const FString& PackageName, UPoseSearchSchema* Schema);

	/**
	 * データベース Database のアニメーションを Sequences（この順）にする（前のものは消す）。null と、スキーマの骨格と違う骨格の
	 * アニメーションは飛ばす（警告を出す）。入れた数を返す。Database かそのスキーマが null なら −1（警告を出す）。保存はしない
	 */
	UFUNCTION(BlueprintCallable, Category = "Tennis|Editor")
	static int32 SetAnimations(UPoseSearchDatabase* Database, const TArray<UAnimSequence*>& Sequences);
};
