// 選手の水平の移動（DD-03 §2.1・§2.2・§2.4）：Engine に依存しない純粋な関数。UTennisMovementComponent の CalcVelocity から呼ぶ。
// 座標はコート局所（m）。選手の側は SideSign（−1：X < 0 の側、+1：X > 0 の側）で表す。
#pragma once

#include "CoreMinimal.h"
#include "FixedStepClock.h"
#include "TennisMovement.generated.h"

/** 移動の値（能力値 50・スタミナ 100% の既定：DD-03 §2.1）。能力値からは TennisMovement::MakeParams で作る。 */
USTRUCT(BlueprintType)
struct TENNISSIM_API FMovementParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.1"))
	double MaxSpeed = 5.5;  // m/s

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.1"))
	double Accel = 8.0;  // m/s²

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.1"))
	double Brake = 10.0;  // m/s²

	/** 90° を超える方向転換で残る速さの割合（35% を失う） */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ClampMax = "1"))
	double TurnKeep = 0.65;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ClampMax = "1"))
	double BackpedalSpeedFactor = 0.75;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ClampMax = "1"))
	double BackpedalAccelFactor = 0.85;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "1"))
	double DashSpeedFactor = 1.08;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ClampMax = "1"))
	double FollowThroughSpeedFactor = 0.40;

	/** 初動ブースト（良いスプリットステップの着地から） */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "1"))
	double BoostAccelFactor = 1.5;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	double BoostSeconds = 0.3;  // s
};

/** 1 ステップの入力のフラグ。 */
struct FMovementModifiers
{
	bool bDash = false;
	bool bFollowThrough = false;
};

/** 水平の移動の状態。 */
struct FMovementState
{
	FVector2D Velocity = FVector2D::ZeroVector;  // m/s（コート局所）
	double    BoostTimeLeft = 0.0;               // s（初動ブーストの残り）
	bool      bTurnLossTaken = false;            // 今の方向転換で損失を受けた（角度が 90° 以下に戻ると解ける）
};

/** 自動移動の目標（DD-03 §2.3）：コート局所の位置 Target へ、到着の時刻（残り TimeLeft 秒）の SettleTime 秒前に止まれるように向かう。 */
struct FAutoMoveTarget
{
	FVector2D Target = FVector2D::ZeroVector;  // m（カプセルの中心）
	double    TimeLeft = 0.0;                  // s（到着の時刻まで。ステップごとに減り、負にもなる）
	double    SettleTime = 0.0;                // s（到着の時刻より前に止まっていたい時間）
};

/**
 * フレームの時間を固定の刻み（SimDt）に直して移動を進める状態（DD-03 §2.2）。位置は外（CMC）が持ち、ここは持たない。
 * 外が渡した速度で動いた後の位置は「最後のステップのモデルの位置 + 今の速度 × まだステップにしていない時間」（範囲に収めたもの）になる。
 */
struct FMovementStepper
{
	FMovementState  State;                         // 最後のステップの後の状態
	FFixedStepClock Clock;
	FVector2D       Lead = FVector2D::ZeroVector;  // m（前のフレームで外に渡した位置 − 最後のステップのモデルの位置）
	TOptional<FAutoMoveTarget> AutoMove;          // あれば、入力の代わりにステップごとに自動移動の入力を使う
};

namespace TennisMovement
{
	inline constexpr double NetGap = 0.15;                  // m（体からネットの面まで：FR-MOVE-006）
	inline constexpr double BodyRadius = 0.30;              // m（カプセルの半径：DD-03 §1）
	inline constexpr double BehindBaseline = 6.0;           // m（ベースラインの後ろへ動ける距離）
	inline constexpr double OutsideDoublesSideline = 3.5;   // m（ダブルスのサイドラインの外へ動ける距離）
	inline constexpr double BackpedalCos = 0.70710678118654752;  // 入力のうちネットから離れる成分がこれ以上なら後退（cos 45°）
	inline constexpr double ArriveTolerance = 0.02;              // m（自動移動の目標にこれより近ければ、着いたとして止まる）

	/** 向き Dir（長さ 1）が後退か：ネットから離れる成分が cos 45° 以上（DD-03 §2.2）。 */
	TENNISSIM_API bool IsBackpedal(const FVector2D& Dir, double SideSign);

	/** 能力値（0〜100 に収める）から移動の値を作る（DD-03 §2.1：スピード → 最高速度、加速 → 加速と減速）。 */
	TENNISSIM_API FMovementParams MakeParams(double SpeedAttribute, double AccelerationAttribute);

	/**
	 * 入力 Input（コート局所の XY。長さが 1 を超えれば 1 に、数でなければ 0 にする）で、Dt 秒進めた状態を返す。
	 * 速度は「入力の向き × 最高速度 × 入力の長さ」へ、速めるときは加速、遅くするときは減速の割合で近づける。
	 * Dt が正でなければ、そのまま返す。
	 */
	TENNISSIM_API FMovementState Step(const FMovementState& State, FVector2D Input, double SideSign, const FMovementModifiers& Modifiers,
		const FMovementParams& Params, double Dt);

	/**
	 * 外の今の位置 Position（カプセルの中心、コート局所の m）から、FrameTime 秒（時間の倍率 TimeScale）を固定の刻みで進め、
	 * 外がこのフレームで動く速度（m/s）を返す。最後のステップのモデルの位置は「Position − 前の Lead」とし、各ステップで範囲に収める
	 * （ClampToArea）。外の位置を毎回の起点にするので、外が動けなかった分（衝突・瞬間移動）は、追いつこうとせずに、そこから続ける。
	 * 入力と Modifiers は、このフレームのすべてのステップに使う（自動移動の間は、入力の代わりにステップごとに AutoMoveInput を使う）。
	 * FrameTime が正でなければ、何も進めずにモデルの速度を返す。
	 */
	TENNISSIM_API FVector2D AdvanceFrame(FMovementStepper& Stepper, const FVector2D& Position, FVector2D Input, double SideSign,
		const FMovementModifiers& Modifiers, const FMovementParams& Params, double FrameTime, double TimeScale = 1.0);

	inline constexpr double MaxPredictTime = 2.0;  // s（PredictPath が先を読む上限。計算の量を抑える：軌跡が使うのは 0.8 s まで。DATA-02 §4.4）

	/**
	 * 軌跡の予測（DD-03 §6.5。T-MOVE-007）：モデルの状態 State と位置 Position（最後のステップのモデルの位置。コート局所の m）から、
	 * AdvanceFrame と同じ 1 ステップ（入力 Input。自動移動 AutoMove があれば、ステップごとに AutoMoveInput。範囲に収める）を固定の刻みで
	 * 進め、Times（s。今からの時刻）の位置を、Times と同じ順で返す。刻みの間の時刻は、前後のステップの位置の線形補間。
	 * 数でない（無限大を含む）・負の時刻は 0、MaxPredictTime を超える時刻は MaxPredictTime にする。時刻 0 は最後のステップの時刻なので、
	 * 呼び出し側は Position に「外の位置 − FMovementStepper::Lead」を渡す。
	 */
	TENNISSIM_API TArray<FVector2D> PredictPath(FMovementState State, FVector2D Position, TOptional<FAutoMoveTarget> AutoMove, FVector2D Input,
		double SideSign, const FMovementModifiers& Modifiers, const FMovementParams& Params, TConstArrayView<double> Times);

	/**
	 * 自動移動の入力（DD-03 §2.3。長さ 0〜1）。目標への向きに、到着の時刻の SettleTime 秒前に着く最小の速さ（加速と減速を含めて見積もる。
	 * 間に合わなければ最高速度）で、
	 * ただし目標で止まれる速さ（√(2·減速·距離)）を超えないように進む。目標から ArriveTolerance 以内なら 0（止まる）。
	 * 速さは入力の長さ × 最高速度（後退なら × BackpedalSpeedFactor）になるように、入力の長さを決める。
	 */
	TENNISSIM_API FVector2D AutoMoveInput(const FVector2D& Position, const FAutoMoveTarget& AutoMove, double SideSign, const FMovementParams& Params);

	/** 初動ブーストを始める（良いスプリットステップの着地：DD-03 §3）。 */
	TENNISSIM_API void StartBoost(FMovementState& State, const FMovementParams& Params);

	/** カプセルの中心 Position を、選手の側 SideSign の範囲に収める。範囲の端に当たった向きの Velocity の成分は 0 にする（DD-03 §2.4）。 */
	TENNISSIM_API void ClampToArea(FVector2D& Position, FVector2D& Velocity, double SideSign);
}
