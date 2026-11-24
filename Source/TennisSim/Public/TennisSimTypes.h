// ボールのシミュレーションの基本の型（DD-01 §1・§2、クラス図 §1）。単位は SI（m・s・kg・rad/s）、座標はコート局所。
// 既定値は DATA-02 の初期値（ITF の Type 2 の球・ハードの中速の面・海面 20 ℃ の空気）。本番の値はデータ（DA_Ball_*・DA_Surface_*）から入れる。
#pragma once

#include "CoreMinimal.h"
#include "TennisSimTypes.generated.h"

/** ボールの状態。予測・本番・ネットワーク・リプレイで同じものを使う。 */
USTRUCT()
struct TENNISSIM_API FBallState
{
	GENERATED_BODY()

	FVector Pos  = FVector::ZeroVector;  // m
	FVector Vel  = FVector::ZeroVector;  // m/s
	FVector Spin = FVector::ZeroVector;  // rad/s（角速度ベクトル）
	double  Time = 0.0;                  // s（シミュレーションの時刻）
	int32   NumBounces = 0;              // 最後の打球からのバウンドの数
	int32   LastHitter = INDEX_NONE;     // 最後に打った選手（まだ誰も打っていなければ INDEX_NONE）
	int32   Seq = 0;                     // 打球の通し番号
	bool    bNetTouched = false;         // 最後の打球からネットに触れたか
	bool    bInPlay = false;
};

/** ボールの物性（DATA-02 §1・§1.1）。データアセット（UBallSpecData）で編集できる。 */
USTRUCT(BlueprintType)
struct TENNISSIM_API FBallParams
{
	GENERATED_BODY()

	static constexpr double Type2RestitutionHard = 0.775;  // 面の e の基準にする球（Type 2）の e_ball（DD-01 §4.1）

	UPROPERTY(EditAnywhere, Category = "Ball")
	double Mass              = 0.0577;  // kg
	UPROPERTY(EditAnywhere, Category = "Ball")
	double Radius            = 0.0335;  // m
	UPROPERTY(EditAnywhere, Category = "Ball")
	double Cd0               = 0.55;    // 抗力係数（回転なし）
	UPROPERTY(EditAnywhere, Category = "Ball")
	double CdSpin            = 0.0;     // 回転による抗力の増分（スピンパラメータ S あたり）
	UPROPERTY(EditAnywhere, Category = "Ball")
	double InertiaAlpha      = 0.55;    // 慣性モーメント I = α m r²
	UPROPERTY(EditAnywhere, Category = "Ball")
	double SpinDecayPerMeter = 0.005;   // 1/m（k_ω）
	UPROPERTY(EditAnywhere, Category = "Ball")
	double RestitutionHard   = Type2RestitutionHard;  // 硬い面での反発係数（e_ball）

	double CrossSectionArea() const { return UE_DOUBLE_PI * Radius * Radius; }  // m²
	double MomentOfInertia() const { return InertiaAlpha * Mass * Radius * Radius; }  // kg·m²
};

/** コート面の性質（DATA-02 §2）。データアセット（UCourtSurfaceData）で編集できる。 */
USTRUCT(BlueprintType)
struct TENNISSIM_API FSurfaceParams
{
	GENERATED_BODY()

	static constexpr double ReferenceNormalSpeed = 8.27;  // m/s（e_ref を決めたときの法線方向の速さ）
	static constexpr double ReferenceTempC       = 23.0;  // ℃
	static constexpr double MinRestitution       = 0.55;
	static constexpr double MaxRestitution       = 0.92;

	UPROPERTY(EditAnywhere, Category = "Surface")
	double RestitutionRef        = 0.80;    // e_ref
	UPROPERTY(EditAnywhere, Category = "Surface")
	double RestitutionSpeedSlope = 0.008;   // 1 m/s あたりの e の減少
	UPROPERTY(EditAnywhere, Category = "Surface")
	double Friction              = 0.63;    // Coulomb の μ
	UPROPERTY(EditAnywhere, Category = "Surface")
	double TangentialRestitution = 0.0;     // e_t（グリップしたときのオーバースピン）
	UPROPERTY(EditAnywhere, Category = "Surface")
	double TempCoeff             = 0.003;   // 1 ℃ あたりの e の変化
	UPROPERTY(EditAnywhere, Category = "Surface")
	double ContactPatchBase      = 0.012;   // m（静止しているときの接地の半径）
	UPROPERTY(EditAnywhere, Category = "Surface")
	double ContactPatchPerVn     = 0.0008;  // m/(m/s)（速いほど潰れて大きい）

	/** 法線方向の速さ Vn（m/s、正の値）と気温での反発係数。物理的にありうる範囲に丸める。 */
	double RestitutionAt(double Vn, double TempC) const
	{
		const double E = RestitutionRef - RestitutionSpeedSlope * (Vn - ReferenceNormalSpeed)
			+ TempCoeff * (TempC - ReferenceTempC);
		return FMath::Clamp(E, MinRestitution, MaxRestitution);
	}

	/** 法線方向の速さ Vn（m/s、正の値）での接地の半径（m）。ラインの判定に使う。 */
	double ContactPatch(double Vn) const { return ContactPatchBase + ContactPatchPerVn * FMath::Max(Vn, 0.0); }
};

/** 空気（DATA-02 §1）。環境（FMatchConditions::ComputeAir）から作る。 */
USTRUCT()
struct TENNISSIM_API FAirParams
{
	GENERATED_BODY()

	static constexpr double GustPeriod1 = 13.0;  // s（DD-01 §10）
	static constexpr double GustPeriod2 = 7.0;   // s

	double  Rho        = 1.204;                // kg/m³（海面 20 ℃）
	FVector WindMean   = FVector::ZeroVector;  // m/s（ショットソルバが知る風はこれだけ：FR-ENV-004）
	double  GustFactor = 0.0;                  // 突風の強さ（平均の風に対する割合、0〜0.5）
	double  GustPhase1 = 0.0;                  // rad（ポイントのシードから）
	double  GustPhase2 = 0.0;                  // rad

	/** 時刻 Time（s）の風：w(t) = w_mean (1 + G (0.6 sin(2πt/13 + φ1) + 0.4 sin(2πt/7 + φ2)))（DD-01 §10）。 */
	FVector WindAt(double Time) const
	{
		if (GustFactor == 0.0)
		{
			return WindMean;  // 突風がなければ三角関数を計算しない（同じ値で速い）
		}
		const double Gust = 0.6 * FMath::Sin(UE_DOUBLE_TWO_PI * Time / GustPeriod1 + GustPhase1)
			+ 0.4 * FMath::Sin(UE_DOUBLE_TWO_PI * Time / GustPeriod2 + GustPhase2);
		return WindMean * (1.0 + GustFactor * Gust);
	}
};

/** 試合の環境（DD-01 §10）。ComputeAir で空気の性質に直す。 */
USTRUCT()
struct TENNISSIM_API FMatchConditions
{
	GENERATED_BODY()

	static constexpr double IndoorTempC = 21.0;  // ℃（屋根を閉じた・屋内の気温）
	static constexpr double MinAltitudeM = -500.0;
	static constexpr double MaxAltitudeM = 4000.0;
	static constexpr double MinTempC = -10.0;
	static constexpr double MaxTempC = 50.0;
	static constexpr double MaxGustFactor = 0.5;

	double  TemperatureC = 20.0;  // ℃
	double  AltitudeM = 0.0;      // m（海面からの高さ）
	FVector WindMean = FVector::ZeroVector;  // m/s（コート局所）
	double  GustFactor = 0.0;  // 突風の強さ（平均の風に対する割合、0〜0.5）
	bool    bRoofClosed = false;

	/** 実際の気温（屋根を閉じれば 21 ℃）。範囲の外は丸め、NaN は既定の 20 ℃。 */
	double EffectiveTempC() const;

	/** 空気（ρ は標高の気圧と気温から。屋根を閉じれば風は 0）。突風の位相は PointSeed から決める（同じシードなら同じ風）。 */
	FAirParams ComputeAir(int32 PointSeed = 0) const;

	/** ρ = p / (287.05 (T + 273.15))、p = 101325 (1 − 2.25577e−5 h)^5.25588（DD-01 §10。ISA の気圧）。 */
	static double AirDensity(double AltitudeM, double TempC);
};

/** 1 ステップを進めるのに必要なもの（コートの幾何・施設・選手の体は、使う作業で加える）。 */
USTRUCT()
struct TENNISSIM_API FBallSimContext
{
	GENERATED_BODY()

	FBallParams    Ball;
	FSurfaceParams Surface;
	FAirParams     Air;
	double         TempC = 20.0;  // ℃（Air.Rho の既定と同じ条件）
};
