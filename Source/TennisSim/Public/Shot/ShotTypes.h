// ショットの型（DD-02、クラス図 §2）：球種、利き手、打つ意図、ソルバの入力と出力。球種の値は ShotTypeParams.h、リクエストへの写し方は ShotRequestMapping.h。
#pragma once

#include "CoreMinimal.h"
#include "TennisSimTypes.h"
#include "ShotTypes.generated.h"

/** 球種（DD-02 §2）。値ごとに DA_Shot_* を 1 つ置く。 */
UENUM(BlueprintType)
enum class ETennisShotType : uint8
{
	Flat,
	Topspin,
	HeavyTopspin,
	Slice,
	Lob,
	SliceLob,
	Drop,
	Volley,
	HalfVolley,
	Smash,
	ServeFlat,
	ServeSlice,
	ServeKick,
	ServeUnderarm,
};

/** 打つ選手の利き手。左利きは回転の軸 φ の符号が逆になる（DD-02 §3.1）。 */
enum class ETennisHandedness : uint8
{
	Right,
	Left,
};

/** 速さ s・仰角 θ に対する 2 つ目の条件（DD-02 §7.1）。球種のデータ（FShotTypeParams）に置くので UENUM。 */
UENUM()
enum class ESolverConstraint : uint8
{
	NetClearance,  // ネットの面を越える瞬間の Z − R − h(Y) = ConstraintValue
	Apex,          // 最高点（ボールの中心の高さ）= ConstraintValue
	FixedSpeed,    // 速さは Speed に固定し、θ だけを解く（サーブ・スマッシュ）
};

/** 解けなかったときの扱い（DD-02 §7.4）。上から順に試す。 */
enum class EShotFallback : uint8
{
	None,             // 収束した
	RelaxConstraint,  // 余裕（または最高点）の目標を半分にして解いた
	SpeedClamped,     // 速さの上限に当たったので、余裕を大きくして解いた
	Nearest,          // 収束しなかった。残差が最小の解を採った（ネットやアウトになりうる）
};

/**
 * 打つ意図（操作仕様 §4.1、ST-05）。プレイヤーの入力も AI もこれを作り、ATennisCharacter::SubmitShotIntent に渡す（ARCH-01 A6）。
 * 時刻はゲームの時刻（s）。溜め（0〜1）は、インパクトでストロークの ChargeTime から求める（TennisShotRequestMapping::ChargeAtImpact）。
 * 狙い・強打・コントロールは M4（FR-INP-004、W-4.2）で加える。
 */
struct FShotIntent
{
	ETennisShotType   Type = ETennisShotType::Topspin;
	TOptional<double> ChargeStartTime;  // s（予約できた＝溜めを始めた時刻。バッファ中は未設定）
	TOptional<double> ReleaseTime;      // s（離した時刻。押している間は未設定）
	bool              bAutoReleased = false;  // 押したままインパクトが来た：t* + W_late に離したものとして判定する（Late の端、溜めは最大。操作仕様 §4.1）
};

struct FShotRequest
{
	FVector Origin = FVector::ZeroVector;  // m（打点。コート局所）
	FVector Target = FVector::ZeroVector;  // m（狙う着地点。Z は使わない）
	double  Time = 0.0;                    // s（インパクトの時刻）

	ESolverConstraint Constraint = ESolverConstraint::NetClearance;
	double ConstraintValue = 1.0;  // m（NetClearance：余裕、Apex：最高点）
	double Speed = 0.0;            // m/s（FixedSpeed のときの速さ）
	double MinSpeed = 10.0;        // m/s（解の範囲の下限の目安）
	double MaxSpeed = 50.0;        // m/s（解の範囲の上限の目安）
	double InitialElevationDeg = 8.0;

	double Rpm = 0.0;          // 回転数（正：トップスピン、負：バックスピン）
	double SpinAxisDeg = 0.0;  // φ（DD-02 §3.1。正：打者から見て左へ曲がる成分）
};

struct FShotSolveResult
{
	bool          bSolved = false;
	FBallState    Launch;                    // Pos = 打点、Vel、Spin、Time = インパクトの時刻
	FVector       PredictedLanding = FVector::ZeroVector;  // m（予測した最初のバウンドの位置）
	double        ResidualRange = 0.0;       // m（着地点と目標の距離）
	double        ResidualConstraint = 0.0;  // m（条件の残差）
	double        PredictedNetClearance = 0.0;
	double        PredictedFlightTime = 0.0; // s
	int32         Iterations = 0;            // ニュートン法の反復の数（方位の補正を通して）
	EShotFallback Fallback = EShotFallback::None;
};
