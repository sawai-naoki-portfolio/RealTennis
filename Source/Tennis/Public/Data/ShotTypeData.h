// 球種のデータ（DATA-01 §3.1、DD-02 §2、クラス図 §2）。DA_Shot_* として球種ごとに 1 つ置く。
// ソルバに渡す値は FShotTypeParams（TennisSim）。σ・球威の再利用・打点の高さ・能力値の項目は、使う M4 で加える。
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Shot/ShotTypeParams.h"
#include "ShotTypeData.generated.h"

UCLASS(BlueprintType)
class TENNIS_API UShotTypeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shot")
	ETennisShotType Type = ETennisShotType::Flat;

	UPROPERTY(EditAnywhere, Category = "Shot")
	FShotTypeParams Params;
};
