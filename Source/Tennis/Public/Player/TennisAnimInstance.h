// 選手のアニメーションの Blueprint（ABP_TennisPlayer）の親（DD-03 §6.5）。Motion Matching に渡す軌跡と、ストローク中に下半身を
// Motion Matching と混ぜる重みを、毎フレーム C++ で作る。値の計算は TennisSim（PredictPath・LowerBodyLocomotionAlpha）。ここが持つのは、
// 過去の位置の履歴（記録と補間）と、標本を並べることだけ。
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/TrajectoryTypes.h"
#include "TennisAnimInstance.generated.h"

UCLASS()
class TENNIS_API UTennisAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Motion Matching の軌跡（SkeletalMeshComponent のワールドの位置と向き。過去・今（時刻 0）・未来）。Pose History のノードの Trajectory に渡す */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Tennis|Locomotion")
	FTransformTrajectory Trajectory;

	/** 下半身を Motion Matching と混ぜる重み（0〜1。1 は腰と脚が Motion Matching。DD-03 §6.5）。振っている間はストロークのステッパから、
	 *  ストロークの外では、モンタージュが動いていれば 0（全身のモンタージュ）、ブレンドアウト中のものだけなら前の値のまま、なければ 1 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Tennis|Locomotion")
	float LowerBodyLocomotionAlpha = 1.0f;

	/** 軌跡の過去の長さ・未来の長さ・標本の間隔（s。アニメの時間。DATA-02 §4.4） */
	UPROPERTY(EditDefaultsOnly, Category = "Tennis|Locomotion", meta = (ClampMin = "0", Units = "s"))
	float HistorySeconds = 0.4f;  // スキーマの軌跡の最も古い標本（−0.4 s）を含む

	UPROPERTY(EditDefaultsOnly, Category = "Tennis|Locomotion", meta = (ClampMin = "0", Units = "s"))
	float PredictionSeconds = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Tennis|Locomotion", meta = (ClampMin = "0.01", Units = "s"))
	float SampleInterval = 0.1f;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	struct FHistorySample
	{
		double Time = 0.0;  // s（AnimTime）
		FVector Position = FVector::ZeroVector;  // cm（メッシュのワールドの位置）
	};

	/** Time（AnimTime）のメッシュの位置。履歴の間は補間、古すぎれば最も古い位置 */
	FVector HistoryAt(double Time) const;

	TArray<FHistorySample> History;  // 古い順
	double AnimTime = 0.0;  // s（アニメの更新の時間の合計。ワールドの時計によらない）
};
