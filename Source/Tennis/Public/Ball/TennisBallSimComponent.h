// ボールのシミュレーションを固定ステップ（240 Hz）で進め、持ち主のアクタをその位置に動かす（DD-01 §3.3〜§3.5）。
// 物理は TennisBallPhysics::Step だけを使う（予測と同じ：D-7）。フレームの時間は FFixedStepClock でステップの数に直す。
#pragma once

#include "CoreMinimal.h"
#include "Ball/BallEvents.h"
#include "Ball/TennisBallPredictor.h"
#include "Components/ActorComponent.h"
#include "FixedStepClock.h"
#include "TennisSimTypes.h"
#include "TennisBallSimComponent.generated.h"

class ATennisCourt;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTennisBallEvent, const FBallEvent&);

UCLASS(ClassGroup = (Tennis), meta = (BlueprintSpawnableComponent))
class TENNIS_API UTennisBallSimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** これより古い打ち出しは警告を出す（s。約 2 フレーム：DD-01 §3.4） */
	static constexpr double LateLaunchWarning = 0.033;

	UTennisBallSimComponent();

	/**
	 * 打ち出す（DD-01 §3.4）。LaunchState.Time が今のシミュレーションの時刻より前なら、今まで早送りし、
	 * その間のイベントも配信する。予測を作り直す。
	 */
	void Launch(const FBallState& LaunchState);

	const FBallState& GetState() const { return State; }

	/** このボールの物理の条件（ソルバも同じ条件で解く：D-7）。 */
	const FBallSimContext& GetContext() const { return Context; }
	const FTrajectory& GetPrediction() const { return Prediction; }

	/** 予測の範囲の中の時刻の状態（DD-01 §3.5）。範囲の外なら false。 */
	bool GetStateAt(double Time, FBallState& Out) const;

	/**
	 * ステップの通し番号：プレイの開始から、このコンポーネントの固定の刻みが数えたステップの数（DD-01 §3.3）。
	 * ボールが飛んでいない間も数える（状態と時刻は、飛んでいる間だけ進む）。ストロークは、この番号の差だけ進む（DD-03 §6.1）。
	 */
	int64 GetStepCount() const { return StepCount; }

	/** 座標の変換に使うコート。 */
	ATennisCourt* GetCourt() const { return Court; }

	/** ボールのイベント（ステップの中の時刻の順）。ルール → 統計 → 演出の順に購読する（DD-01 §8）。 */
	FOnTennisBallEvent OnBallEvent;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** 座標の変換に使うコート。空ならプレイの開始時にレベルの中から探す。 */
	UPROPERTY(EditAnywhere, Category = "Tennis")
	TObjectPtr<ATennisCourt> Court;

private:
	void StepOnce();
	void Broadcast(const TArray<FBallEvent>& Events);
	void UpdateOwnerLocation() const;
	void DrawDebug() const;

	FBallSimContext Context;
	FBallState State;
	FBallState PrevState;
	FTrajectory Prediction;
	FFixedStepClock Clock;
	int64 StepCount = 0;            // ステップの通し番号（飛んでいない間も数える）
	TArray<FBallEvent> StepEvents;  // 1 ステップのイベント（毎回確保しないため）
	TArray<FVector> ActualPath;     // 打ち出しからの本番の位置（コート局所。デバッグの描画だけに使う）
	bool bInFlight = false;
};
