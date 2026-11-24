// ストロークの選択（DD-03 §5）：打点の候補と球種から Chooser の表の入力を作り、表を評価して UStrokeData を返す。
// Chooser（UChooserTable::EvaluateChooser）を呼ぶのは、ここの Select の 1 か所だけにする。
#pragma once

#include "CoreMinimal.h"
#include "Reach/TennisReach.h"
#include "Shot/ShotTypes.h"
#include "Stroke/TennisStrokeSelect.h"
#include "TennisStrokeSelection.generated.h"

class UChooserTable;
class UStrokeData;

/** Chooser の表 CHT_StrokeSelect の入力（Struct Parameter）。列はこのプロパティに束縛する（Blueprint から見えるものだけ束縛できる） */
USTRUCT(BlueprintType)
struct FTennisStrokeChooserInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stroke")
	EStrokeSide Side = EStrokeSide::Forehand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stroke")
	EBackhandStyle BackhandStyle = EBackhandStyle::TwoHanded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stroke")
	EStrokeHeightBand Height = EStrokeHeightBand::Mid;

	/** 候補の打点のボールの中心の高さ（m。Height の帯と同じ z）。同じ帯の中の高さの変種を選ぶ範囲の列で使う（DD-03 §5.3） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stroke")
	double HitHeight = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stroke")
	EStrokeReach Reach = EStrokeReach::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stroke")
	EStrokeFamily Family = EStrokeFamily::Drive;

	/** 決め方は、スタンスの違うモンタージュを入れるとき（M9）に足す。それまでは Square */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stroke")
	EStrokeStance Stance = EStrokeStance::Square;

	/** 選ぶ時刻からインパクトまでの時間（s） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stroke")
	float TimeBudget = 0.0f;
};

namespace TennisStrokeSelection
{
	/** 打点の候補・球種・バックハンドの打ち方・今の時刻（s）から、表の入力を作る（決め方は TennisStrokeSelect：DD-03 §5.1） */
	TENNIS_API FTennisStrokeChooserInput MakeInput(const FHitCandidate& Candidate, ETennisShotType ShotType, EBackhandStyle BackhandStyle,
		double NowSeconds, double BlockTimeBudgetSeconds = TennisStrokeSelect::DefaultBlockTimeBudget);

	/** MidDriveInput の打点の高さ（m）。高さの変種（DD-03 §5.3）では、上の行が選ばれる高さ */
	inline constexpr double MidDriveHitHeight = 1.0;

	/** 探索の体の位置（DD-03 §4.2 の ReachBody）を作る、中くらいの高さ（MidDriveHitHeight）・通常の届き方のドライブの入力（T-STRK-011 と同じ） */
	TENNIS_API FTennisStrokeChooserInput MidDriveInput(EStrokeSide Side, EBackhandStyle BackhandStyle);

	/** 表を評価して、最初に合った行の UStrokeData を返す。表がない・どの行も合わなければ nullptr（警告を出す。そのストロークは振らない） */
	TENNIS_API UStrokeData* Select(const UChooserTable* Table, FTennisStrokeChooserInput& Input);
}
