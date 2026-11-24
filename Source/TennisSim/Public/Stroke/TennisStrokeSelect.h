// ストロークの選択の入力の決め方（DD-03 §5.1）：Engine に依存しない純粋な関数。打点の候補と打つ意図から、
// Chooser の表（CHT_StrokeSelect）に渡す列挙の値を決める。列挙は表の列に束縛するので BlueprintType（RES-06 G-7）。
#pragma once

#include "CoreMinimal.h"
#include "Reach/TennisReach.h"
#include "Shot/ShotTypes.h"
#include "TennisStrokeSelect.generated.h"

/** 打点のボールの高さの帯（アニメーションの高さの違い。ART-02 §2） */
UENUM(BlueprintType)
enum class EStrokeHeightBand : uint8
{
	VeryLow,   // z < 0.4 m
	Low,       // 0.4 ≤ z < 0.7
	Mid,       // 0.7 ≤ z < 1.3
	High,      // 1.3 ≤ z ≤ 2.2（ボレーの帯の上端まで）
	Overhead,  // z > 2.2（スマッシュの帯）
};

/** 打点への届き方。Dive・Slide は、その動きを足すとき（M6 以降）に決め方を足す */
UENUM(BlueprintType)
enum class EStrokeReach : uint8
{
	Normal,
	Stretch,
	Dive,
	Slide,
};

/** 振り方の系統 */
UENUM(BlueprintType)
enum class EStrokeFamily : uint8
{
	Drive,
	Slice,
	Lob,
	Drop,
	Volley,
	HalfVolley,
	Smash,
	Block,
};

/** スタンス。決め方は、スタンスの違うモンタージュを入れるとき（M9）に足す。それまで入力は Square */
UENUM(BlueprintType)
enum class EStrokeStance : uint8
{
	Open,
	SemiOpen,
	Square,
	Closed,
};

namespace TennisStrokeSelect
{
	/** 高さの帯の境界（m。DATA-02 §4.1） */
	inline constexpr double VeryLowBelow = 0.4;
	inline constexpr double LowBelow = 0.7;
	inline constexpr double MidBelow = 1.3;
	inline constexpr double HighUpTo = 2.2;

	/** グラウンドストロークを Block にする準備時間の初期値（s。これより短ければ Block。DATA-02 §4.1：推定） */
	inline constexpr double DefaultBlockTimeBudget = 0.30;

	/** 打点のボールの中心の高さ（m）の帯。数でない入力は Mid（警告を出す） */
	TENNISSIM_API EStrokeHeightBand HeightBandOf(double BallHeightM);

	/**
	 * 振り方の系統。候補の種類が Volley・HalfVolley・Smash なら、その種類。Groundstroke は球種で決める
	 * （Flat・Topspin・HeavyTopspin → Drive、Slice → Slice、Lob・SliceLob → Lob、Drop → Drop）。
	 * Groundstroke で準備時間 TimeBudgetS が BlockTimeBudgetS より短ければ Block。
	 * Groundstroke に合わない球種（サーブなど）は Drive（警告を出す）。
	 */
	TENNISSIM_API EStrokeFamily FamilyOf(EHitKind Kind, ETennisShotType Type, double TimeBudgetS, double BlockTimeBudgetS);

	/** 候補がストレッチなら Stretch、それ以外は Normal */
	TENNISSIM_API EStrokeReach ReachOf(bool bStretch);
}
