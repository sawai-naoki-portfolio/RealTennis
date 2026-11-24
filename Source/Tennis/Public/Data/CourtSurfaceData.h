// コート面のデータ（DATA-02 §2、クラス図 §3）。DA_Surface_* として置き、新しい面はアセットを足すだけで増やす（OCP）。
// 音・エフェクト・ボールマークなどの演出の項目は、使うマイルストーン（M7）で加える。
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TennisSimTypes.h"
#include "CourtSurfaceData.generated.h"

UCLASS(BlueprintType)
class TENNIS_API UCourtSurfaceData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 校正で許す CPR の差（ITF の目標の ±2：RES-02 §6 の T-BALL-004） */
	static constexpr double CprTolerance = 2.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	FText DisplayName;

	/** 目標の Court Pace Rating（DATA-02 §2）。校正のツールが、仮想 CPR 試験の値がこの ±2 に入るかを確かめる */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface", meta = (ClampMin = "0", ClampMax = "100"))
	double TargetCpr = 38.5;

	UPROPERTY(EditAnywhere, Category = "Surface")
	FSurfaceParams Params;
};
