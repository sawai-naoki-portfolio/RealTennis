// 球種の値のうち、ソルバに渡すもの（DATA-01 §3.1、DD-02 §2）。UShotTypeData（DA_Shot_*）が持つ。
// σ・球威の再利用・打点の高さ・能力値の項目は、使う M4 で加える。
#pragma once

#include "CoreMinimal.h"
#include "Shot/ShotTypes.h"
#include "ShotTypeParams.generated.h"

USTRUCT(BlueprintType)
struct TENNISSIM_API FShotTypeParams
{
	GENERATED_BODY()

	/** m/s（速さの範囲。速さを固定する球種は、溜めで決める速さの範囲） */
	UPROPERTY(EditAnywhere, Category = "Shot", meta = (ClampMin = "5", ClampMax = "70"))
	double MinSpeed = 25.0;

	UPROPERTY(EditAnywhere, Category = "Shot", meta = (ClampMin = "5", ClampMax = "70"))
	double MaxSpeed = 46.0;

	/** rpm（正：トップスピン、負：バックスピン） */
	UPROPERTY(EditAnywhere, Category = "Shot", meta = (ClampMin = "-4000", ClampMax = "4500"))
	double BaseRpm = 600.0;

	/** 度（回転の軸 φ。右利きの値。DD-02 §3.1） */
	UPROPERTY(EditAnywhere, Category = "Shot", meta = (ClampMin = "-90", ClampMax = "90"))
	double SpinAxisDeg = 0.0;

	UPROPERTY(EditAnywhere, Category = "Shot")
	ESolverConstraint Constraint = ESolverConstraint::NetClearance;

	/** m（溜め 0 と 1 のときの条件の目標。NetClearance：ネットの上の余裕、Apex：最高点。FixedSpeed では使わない） */
	UPROPERTY(EditAnywhere, Category = "Shot", meta = (ClampMin = "0", ClampMax = "12"))
	double ConstraintAtCharge0 = 0.70;

	UPROPERTY(EditAnywhere, Category = "Shot", meta = (ClampMin = "0", ClampMax = "12"))
	double ConstraintAtCharge1 = 0.25;

	/** 度（ソルバの初期値） */
	UPROPERTY(EditAnywhere, Category = "Shot", meta = (ClampMin = "-25", ClampMax = "60"))
	double InitialElevationDeg = 5.0;
};
