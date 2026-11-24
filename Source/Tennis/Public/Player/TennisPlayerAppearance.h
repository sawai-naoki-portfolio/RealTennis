// 選手の見た目のデータ（DD-03 §7）：体（メインのメッシュ）と、体の骨に追従する部品（顔・服）と、髪（Groom の Binding）。
// 体で決まる値（ストロークのスイートスポット・探索の体の位置）は、その体で焼いて持つ（UTennisStrokeMetaBaker::BakeAppearance）。
// 仮の選手（ART-01 A-CHR-004・005）は体だけ、MetaHuman の選手（A-CHR-010・011）は体・顔・服・髪。選手ごとの選択は M9（DA_Player_*）。
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TennisPlayerAppearance.generated.h"

class UGroomBindingAsset;
class USkeletalMesh;
class UStrokeData;

/** あるストロークの、この体でのインパクトの瞬間のスイートスポット（焼いた値。DD-03 §7） */
USTRUCT(BlueprintType)
struct TENNIS_API FTennisStrokeFit
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Baked")
	TSoftObjectPtr<UStrokeData> Stroke;

	/** アクターの空間（足元が原点、+X が前、+Y が右）で見たラケットのスイートスポット。位置は m */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Baked")
	FTransform SweetSpotOffset;

	/** Fits の中の Stroke の値。なければ nullptr */
	static const FTennisStrokeFit* Find(const TArray<FTennisStrokeFit>& Fits, const UStrokeData* Stroke);
};

/** この体での、打点の探索の体の位置（DD-03 §4.2。m。焼いた値。FReachParams の同じ名前の値に入れる） */
USTRUCT(BlueprintType)
struct TENNIS_API FTennisReachBody
{
	GENERATED_BODY()

	/** 前の距離：中くらいの高さのドライブの 3 つの入力（フォア・片手のバック・両手のバック）で選ばれるストロークの、
	 *  スイートスポットの前の位置の平均（入力ごとの平均。同じストロークが 2 つの入力で選ばれれば 2 回数える） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Baked")
	double Forward = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Baked")
	double ForehandSide = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Baked")
	double OneHandedBackhandSide = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Baked")
	double TwoHandedBackhandSide = 0.0;

	/** 焼いてある（どの値も正の数） */
	bool IsBaked() const;
};

UCLASS(BlueprintType)
class TENNIS_API UTennisPlayerAppearance : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 体（メインのメッシュ。ラケットのソケット racket_attach_r・_l を持つ骨格：ART-01 §3.1） */
	UPROPERTY(EditAnywhere, Category = "Appearance")
	TSoftObjectPtr<USkeletalMesh> Body;

	/** 体の骨に追従する部品（顔・服）。それぞれ USkeletalMeshComponent にし、Leader Pose を体にする */
	UPROPERTY(EditAnywhere, Category = "Appearance")
	TArray<TSoftObjectPtr<USkeletalMesh>> Followers;

	/** 髪・眉・ひげ（Groom の Binding）。Groom と付ける先のメッシュ（MetaHuman では顔）は Binding が持つ。
	 *  それぞれ UGroomComponent にし、Binding の対象のメッシュを持つ部品（追従する部品か体）に付ける */
	UPROPERTY(EditAnywhere, Category = "Appearance")
	TArray<TSoftObjectPtr<UGroomBindingAsset>> GroomBindings;

	/** すべての DA_Stroke_* の、この体でのスイートスポット（ストロークの名前の順。手で入れない） */
	UPROPERTY(VisibleAnywhere, Category = "Baked")
	TArray<FTennisStrokeFit> StrokeFits;

	/** この体での探索の体の位置（手で入れない） */
	UPROPERTY(VisibleAnywhere, Category = "Baked")
	FTennisReachBody ReachBody;

	/** Stroke の焼いた値。なければ nullptr（そのストロークは振らない：DD-03 §6.6） */
	const FTennisStrokeFit* FindStrokeFit(const UStrokeData* Stroke) const;
};
