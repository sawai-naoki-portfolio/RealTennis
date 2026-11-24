// 選手の移動（DD-03 §2）：床・衝突・高さは CMC に任せ、水平の速度は TennisSim の TennisMovement で計算する（CalcVelocity）。
// 移動のモデルは固定の刻み（240 Hz）で進め、選手の側の範囲（ネットの 0.15 m 手前など）にもステップごとに収める。
// CMC には、そのフレームで動く速度を渡す（TennisMovement::AdvanceFrame）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Movement/TennisMovement.h"
#include "TennisMovementComponent.generated.h"

class ATennisCourt;

UCLASS(ClassGroup = (Tennis))
class TENNIS_API UTennisMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UTennisMovementComponent();

	/** 初動ブーストを始める（良いスプリットステップの着地：DD-03 §3）。 */
	void StartBoost();

	/**
	 * 自動移動を始める（DD-03 §2.3）：コート局所の位置 CourtTarget（m、カプセルの中心）へ、TimeLeft 秒後の到着の SettleTime 秒前に
	 * 止まれるように向かう。止めるまで、スティックの入力は使わない。
	 */
	void SetAutoMoveTarget(const FVector2D& CourtTarget, double TimeLeft, double SettleTime);
	void ClearAutoMoveTarget();
	bool IsAutoMoving() const { return Stepper.AutoMove.IsSet(); }

	/** フォロースルーの移動の制限（最高速度 ×FollowThroughSpeedFactor：DD-03 §2.1）を付ける・外す。ストロークがインパクトから RecoverStart まで付ける（§6.6） */
	void SetFollowThrough(bool bOn) { Modifiers.bFollowThrough = bOn; }
	const FMovementModifiers& GetModifiers() const { return Modifiers; }

	/**
	 * コートの座標 CourtTransform で、カプセルの中心のワールドの位置 WorldLocation（cm）と入力 WorldInput（ワールドの向き、長さ 1 以下）
	 * から、FrameTime 秒（時間の倍率 TimeScale）のフレームで動く速度（cm/s）を求める。水平だけで、Z は WorldVelocity のまま。
	 * モデルは Stepper の固定の刻みで進む。SideSign は選手の側（−1：X < 0）。Modifiers は移動の制限（フォロースルーなど）。
	 */
	static FVector AdvanceWorldVelocity(const FTransform& CourtTransform, const FVector& WorldLocation, const FVector& WorldVelocity,
		const FVector& WorldInput, double SideSign, FMovementStepper& Stepper, const FMovementParams& Params, double FrameTime, double TimeScale,
		const FMovementModifiers& Modifiers = FMovementModifiers());

	/** 移動の値（能力値 50 の既定。選手のデータから TennisMovement::MakeParams で入れる）。 */
	UPROPERTY(EditAnywhere, Category = "Tennis")
	FMovementParams Params;

	virtual void BeginPlay() override;

	/** 座標の基準のコート（プレイの開始時に決まる。なければ nullptr） */
	ATennisCourt* GetCourt() const { return Court; }
	/** 選手の側（−1：X < 0 の側、+1：X > 0 の側） */
	double GetSideSign() const { return SideSign; }
	const FMovementState& GetMoveState() const { return Stepper.State; }

	/**
	 * 軌跡の予測（DD-03 §6.5）：今のモデルの状態から、このフレームと同じ入力・修飾（自動移動の間は自動移動）で進めた、今から Times 秒後
	 * （シミュレーションの時間）のカプセルの中心のワールドの位置（cm）。TennisMovement::PredictPath を、最後のステップの位置と、まだ
	 * ステップにしていない時間を足した時刻で呼ぶ（Times が 0 なら今の位置）。コートがなければ空
	 */
	TArray<FVector> PredictWorldPath(TConstArrayView<double> Times) const;

protected:
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;

	/** 座標の基準のコート。空ならプレイの開始時にレベルの中から探す。 */
	UPROPERTY(EditAnywhere, Category = "Tennis")
	TObjectPtr<ATennisCourt> Court;

private:
	/** 移動の入力（ワールドの向き × スティックの倒し具合。CalcVelocity と PredictWorldPath が同じものを使う） */
	FVector CurrentInput() const;

	FMovementStepper Stepper;
	FMovementModifiers Modifiers;
	double SideSign = -1.0;  // 選手の側（プレイの開始時の位置で決める。ネットを越えないので変わらない）
};
