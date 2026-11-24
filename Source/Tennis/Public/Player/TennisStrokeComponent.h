// ストロークを振る（DD-03 §6.6）。打点の候補でストロークを選び（§5.3）、打点の時刻を選び直し（§6.2）、ボールのステップの通し番号の差だけ
// ステッパ（TennisStrokeStep）を進めて（§6.1 の時計）、止めたモンタージュの位置と体の向きを合わせる。インパクトと RecoverStart を知らせる。
// 計算は TennisSim（TennisStrokeTiming・TennisStrokeStep・TennisReach）にあり、ここは結果を体とモンタージュに渡すだけ（S-2）。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Reach/TennisReach.h"
#include "Shot/ShotTypes.h"
#include "Stroke/TennisStrokeStepper.h"
#include "Stroke/TennisStrokeTiming.h"
#include "TennisStrokeComponent.generated.h"

class ATennisCharacter;
class ATennisCourt;
class UStrokeData;
class UTennisBallSimComponent;

/** インパクト（DD-03 §6.4）：時刻 T と、T のスイートスポットの位置 */
struct FStrokeImpact
{
	double  Time = 0.0;                        // s（ボールのシミュレーション時刻。ステップの端数まで）
	FVector SweetSpot = FVector::ZeroVector;   // m（コート局所。T の体の位置と向き + 焼いた値：§6.4 の 5）
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnStrokeImpact, const FStrokeImpact&);

UCLASS(ClassGroup = (Tennis))
class TENNIS_API UTennisStrokeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTennisStrokeComponent();

	/** 同期の値（DATA-02 §4.2） */
	UPROPERTY(EditAnywhere, Category = "Tennis")
	FStrokeSyncParams Sync;

	/**
	 * 打点の候補 Candidate と球種 ShotType でストロークを始める（DD-03 §6.6）。表でストロークを選び、打点を選び直し、自動移動の目標を置き直し、
	 * ステッパを始め、モンタージュを止めた状態で再生する。振っている途中（インパクトの前でも、後のフォロースルーでも）なら、前のものを知らせずに
	 * 打ち切って（フォロースルーの制限も外す）始める。選べない・始められない（ボール・選手・アニメがない、データがおかしい、モンタージュを
	 * 再生できない）なら、警告を出して何もせずに（前のストロークもそのまま）false
	 */
	bool PlayStroke(const FHitCandidate& Candidate, ETennisShotType ShotType);

	/**
	 * 取り消す（DD-03 §6.6）。インパクトの前なら止めて（モンタージュも止める）true。インパクトの後は取り消さずに false を返し、終わりまで進める
	 * （そのときも OnFinished を知らせる）。止めた後は、体の向きを毎フレーム、ネットの方向へ Sync.ReturnYawRateDeg で戻す（DD-03 §6.5）。
	 * PlayStroke が置いた自動移動の目標は消さない（呼び出し側が止める）
	 */
	bool CancelStroke();

	bool IsStroking() const { return Stepper.State != EStrokeState::None; }
	const FStrokeStepper& GetStepper() const { return Stepper; }
	/** 選び直した打点（打点の時刻はボールのシミュレーション時刻） */
	const FHitCandidate& GetPlannedHit() const { return Hit; }
	const UStrokeData* GetStroke() const { return Stroke; }

	/**
	 * インパクト（ステッパの位置がインパクトの時刻を越えた：DD-03 §6.4）。知らせる前に、自動移動を止め、フォロースルーの制限を付ける。
	 * 長いフレームでインパクトと RecoverStart が同じ Tick に来たときは、知らせる時点で制限はもう外れている（OnFinished がすぐ続く。
	 * 受けた側が次のストロークを始めたら、前のものの OnFinished は知らせない）
	 */
	FOnStrokeImpact OnImpact;
	/** RecoverStart に達した（ストロークの終わり。空振りも同じ）。振っている途中にボール・移動がなくなって打ち切ったときも知らせる。
	 *  知らせる前に、フォロースルーの制限を外す */
	FSimpleMulticastDelegate OnFinished;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	ATennisCharacter* GetCharacter() const;
	/** 選手の移動のコート。なければ null */
	const ATennisCourt* GetCourt() const;
	const UTennisBallSimComponent* GetBall() const;
	/** ステッパのモンタージュの位置と体の向きを、モンタージュと選手に渡す */
	void ApplyToBody() const;
	/** 体の向きをコート局所のヨー Yaw（rad）にする。コートがなければ何もしない */
	void SetBodyYaw(double Yaw) const;
	/** 取り消した後、体の向きを DeltaTime（ワールドの秒。時間の倍率を掛ける）の分だけネットの方向へ戻す。まだ戻り切っていなければ true。
	 *  戻す速さが正でない・コートがないときは false（戻さずに止める） */
	bool TurnBackToNet(float DeltaTime);
	/** 今のストロークを、知らせずに打ち切る（モンタージュを止め、制限を外す） */
	void StopStroke();

	UPROPERTY(Transient)
	TObjectPtr<UStrokeData> Stroke;

	FStrokeStepper Stepper;
	FHitCandidate Hit;
	FVector ImpactOffset = FVector::ZeroVector;  // m（利き手に合わせた、このストロークの焼いた値：§6.4 の 5）
	int64 LastStepCount = 0;  // 前に読んだボールのステップの通し番号
	double NetYaw = 0.0;      // rad（コート局所。ネットの方向）
	bool bReturningToNet = false;  // 取り消した後、体の向きをネットへ戻している
};
