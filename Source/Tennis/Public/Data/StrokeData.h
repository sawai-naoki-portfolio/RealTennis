// ストロークのデータ（DD-03 §5.2、クラス図 §4）。DA_Stroke_* として置き、新しいストロークはアセットを足すだけで増やす（OCP）。
// 選ばれる条件（SelectRows）はここにだけ書き、Chooser の表 CHT_StrokeSelect はエディタのツールがここから作る（DD-03 §5.3）。
// 焼いた値（ImpactTime など）は、エディタのツール StrokeMetaBaker がモンタージュから書く（手で入れない）。インパクトのスイートスポットは
// 体ごとに違うので、ここには持たず、選手の見た目のデータ（UTennisPlayerAppearance::StrokeFits）に焼く（DD-03 §5.2・§7）。
// 離す時刻の先行時間（ReleaseLeadTime）と溜めの時間（ChargeTime）は手で入れる（自動の離し：DD-03 §6.6、打球の溜め：§6.4 が読む）。
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Reach/TennisReach.h"
#include "Stroke/TennisStrokeSelect.h"
#include "StrokeData.generated.h"

class UAnimMontage;

/** 選ばれる条件の 1 行。配列が空の項目は Any。複数の値を入れた項目は、その数だけの行に展開する */
USTRUCT(BlueprintType)
struct FStrokeSelectRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Select")
	EStrokeSide Side = EStrokeSide::Forehand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Select")
	TArray<EBackhandStyle> BackhandStyles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Select")
	TArray<EStrokeHeightBand> Heights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Select")
	TArray<EStrokeReach> Reaches;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Select")
	TArray<EStrokeFamily> Families;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Select")
	TArray<EStrokeStance> Stances;

	/** 打点の高さ（m）の範囲：下限を含み上限を含まない（境界は float にした値。DD-03 §5.3 の高さの変種）。チェックを外した側は範囲なし。
	 *  高さの変種は片側だけにする（両端があると、上限のちょうどの値を含んでしまう） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Select", meta = (InlineEditConditionToggle))
	bool bHasHitHeightMin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Select", meta = (EditCondition = "bHasHitHeightMin", Units = "m"))
	double HitHeightMin = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Select", meta = (InlineEditConditionToggle))
	bool bHasHitHeightMax = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Select", meta = (EditCondition = "bHasHitHeightMax", Units = "m"))
	double HitHeightMax = 0.0;
};

UCLASS(BlueprintType)
class TENNIS_API UStrokeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** ストロークのモンタージュ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stroke")
	TObjectPtr<UAnimMontage> Montage;

	/** 選ばれる条件。どれか 1 行に合えば選ばれる。条件の多い行が先に試される */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stroke")
	TArray<FStrokeSelectRow> SelectRows;

	/**
	 * 理想の離す時刻の、インパクトからの先行時間（s。手動。t* = インパクトの時刻 − ReleaseLeadTime）。値は操作仕様 §4.1 の表。
	 * ボタンを離さなければ、t* + W_late で離したことにする（DD-03 §6.6 の自動の離し）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stroke", meta = (ClampMin = "0", Units = "s"))
	double ReleaseLeadTime = 0.0;

	/**
	 * 溜めが 0 から 1 になる時間（s。手動）。打球の溜め =（離した時刻 − 溜めの始まり）÷ ChargeTime を 0〜1 に収めたもの（DD-03 §6.4 の 6）。
	 * 値は操作仕様 §4.1 の表
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stroke", meta = (ClampMin = "0", Units = "s"))
	double ChargeTime = 0.0;

	/** モンタージュの先頭からインパクトまで（s）。Notify「Tennis Impact」の時刻 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Baked")
	double ImpactTime = 0.0;

	/** 打った後、移動を制限する時間（s）。インパクトから Notify「Tennis Recover Start」まで */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Baked")
	double FollowThroughLock = 0.0;
};
