// ボールのデータ（DATA-02 §1.1、クラス図 §3）。ITF の球の Type ごとに DA_Ball_* として置く（OCP）。
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TennisSimTypes.h"
#include "BallSpecData.generated.h"

UCLASS(BlueprintType)
class TENNIS_API UBallSpecData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ball")
	FText DisplayName;

	UPROPERTY(EditAnywhere, Category = "Ball")
	FBallParams Params;
};
