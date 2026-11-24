// ストロークの同期の計算（DD-03 §6.1・§6.2）：Engine に依存しない純粋な関数。モンタージュのインパクトをボールの打点の時刻に
// 合わせる始め方（待つ時間・開始位置・再生速度）と再生中の補正、体の向きと体の目標位置。時間は s（ボールのシミュレーション時刻）、位置はコート局所（m）。
#pragma once

#include "CoreMinimal.h"
#include "TennisStrokeTiming.generated.h"

/** 同期の値（DATA-02 §4.2・§4.4）。 */
USTRUCT(BlueprintType)
struct TENNISSIM_API FStrokeSyncParams
{
	GENERATED_BODY()

	/** 再生速度の範囲（FR-STRK-003。検証 T-STRK-012・T-STRK-004） */
	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "0.1"))
	double MinPlayRate = 0.80;

	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "0.1"))
	double MaxPlayRate = 1.25;

	/** 再生中に、打点の時刻を読み直して再生速度を補正する間隔（s） */
	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "0"))
	double ReplanInterval = 0.05;

	/** 再生速度の補正の上限（1 秒あたり） */
	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "0"))
	double MaxRateChangePerSecond = 0.5;

	/** 打点の時刻を選び直す範囲：候補の時刻の前後（s。DD-03 §6.2。検証 T-STRK-013・T-STRK-003） */
	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "0"))
	double HitRefineWindow = 0.25;

	/** 体の向きの限界：ネットの方向からの角度（度。FR-STRK-004） */
	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0", ClampMax = "90", Units = "deg"))
	double MaxFacingDeg = 35.0;

	/** M3 の狙い：相手のコートの中央の、ネットからの距離（m。操作仕様 §4.3 のニュートラル。DD-03 §6.4。M4 で狙いの入力に置き換える） */
	UPROPERTY(EditAnywhere, Category = "Aim", meta = (ClampMin = "0", Units = "m"))
	double NeutralAimDepth = 9.0;

	/** インパクトで打つ距離：スイートスポットとボールの中心の距離がこれ以下なら打球、より遠ければ空振り（m。DD-03 §6.4 の 4。検証 T-STRK-016・T-STRK-003） */
	UPROPERTY(EditAnywhere, Category = "Impact", meta = (ClampMin = "0", Units = "m"))
	double ImpactReach = 0.25;

	/** スイングの始まり：インパクトのこれだけ前から、下半身も全身のモンタージュにする（s。DD-03 §6.5。検証 T-STRK-018） */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0", Units = "s"))
	double SwingLead = 0.30;

	/** 下半身の重みを 1 から 0 にする時間（s。スイングの始まりの手前。DD-03 §6.5） */
	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "0", Units = "s"))
	double LowerBodyBlendTime = 0.10;

	/** 取り消した後、体の向きをネットの方向へ戻す速さ（度/秒。CMC の RotationRate と同じ。DD-03 §6.5。検証 T-STRK-019） */
	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0", Units = "deg/s"))
	double ReturnYawRateDeg = 720.0;
};

/** モンタージュの始め方。インパクトは「Delay + (ImpactTime − StartPosition) / PlayRate」秒後になる。 */
struct FStrokeStartPlan
{
	double Delay = 0.0;          // s（再生を始めるまで待つ時間）
	double StartPosition = 0.0;  // s（モンタージュの開始位置。テイクバックを省くときに正）
	double PlayRate = 1.0;
};

namespace TennisStrokeTiming
{
	/**
	 * 角度（rad）を −π より大きく π 以下にする。エンジンの UnwindRadians・FindDeltaAngleRadians は単精度の π を使い、
	 * double の角度に 1e-7 rad の誤差が入るので、TennisSim ではこれを使う。
	 */
	TENNISSIM_API double WrapAngle(double Angle);

	/** ネットの方向（コート局所のヨー、rad）：手前の選手（SideSign < 0）は 0、奥の選手は π */
	TENNISSIM_API double NetYaw(double SideSign);

	/** M3 のニュートラルの狙い（コート局所の m）：相手のコートの中央、ネットから NeutralAimDepth（DD-03 §6.4）。SideSign は打つ選手の側 */
	TENNISSIM_API FVector2D NeutralAim(double SideSign, const FStrokeSyncParams& Params);

	/**
	 * モンタージュのインパクトの時刻 ImpactTime（先頭から、s）を、Available 秒後に合わせる始め方（DD-03 §6.1）。
	 * 再生速度が範囲に収まればそのまま。速すぎるときは上限にして途中から再生し、遅すぎるときは下限にして開始を遅らせる。
	 * Available が 0 以下（打点の時刻を過ぎた）なら、インパクトの位置から上限の速度で始める。
	 * ImpactTime が正でない・数でない入力は、待たずに先頭から等倍（警告を出す）。
	 */
	TENNISSIM_API FStrokeStartPlan PlanStart(double ImpactTime, double Available, const FStrokeSyncParams& Params);

	/**
	 * 再生中の補正：モンタージュの今の位置 Position から、インパクトが Available 秒後になる再生速度へ、CurrentRate から
	 * Elapsed 秒の分（MaxRateChangePerSecond × Elapsed）だけ近づけた値。範囲に収める。インパクトを過ぎていれば CurrentRate のまま。
	 * CurrentRate が数でなければ、等倍から始める（警告を出す）。
	 */
	TENNISSIM_API double CorrectPlayRate(double CurrentRate, double Position, double ImpactTime, double Available, double Elapsed,
		const FStrokeSyncParams& Params);

	/**
	 * 体の向き（コート局所のヨー、rad。+X が 0、+Y へ回る向きが正。−π より大きく π 以下）：打点 HitPoint から狙いの地点 AimPoint への向き。
	 * ただし、ネットの方向（SideSign < 0 の選手は +X、それ以外は −X）から ±MaxFacingDeg まで（DD-03 §6.2）。
	 * 向きが決まらない（同じ点・数でない）ときは、ネットの方向。
	 */
	TENNISSIM_API double FacingYaw(const FVector2D& HitPoint, const FVector2D& AimPoint, double SideSign, const FStrokeSyncParams& Params);

	/**
	 * 体の目標位置（カプセルの中心の水平の位置）：ヨー YawRad（rad）を向いた体の、スイートスポットの水平のずれ
	 * SweetSpotOffset（X が前、Y が右。m）が、ボールの位置 BallPos に重なる位置（DD-03 §6.2）。
	 */
	TENNISSIM_API FVector2D BodyTarget(const FVector2D& BallPos, double YawRad, const FVector2D& SweetSpotOffset);

	/**
	 * 体の向き Yaw を TargetYaw へ、短い向きに、Dt 秒で RateRad × Dt（rad）まで近づける（越えない。DD-03 §6.5 の取り消した後の向き）。
	 * 戻り値は −π より大きく π 以下。TargetYaw が数でなければ Yaw のまま（Yaw も数でなければ 0）、Yaw が数でなければ TargetYaw（どちらも警告を出す）。
	 * 近づける量が正でなければ（数でないときも）、動かさない。
	 */
	TENNISSIM_API double ReturnYaw(double Yaw, double TargetYaw, double RateRad, double Dt);
}
