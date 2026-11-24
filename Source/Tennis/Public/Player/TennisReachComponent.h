// 打点の探索（DD-03 §4.4）：選手の位置・速度とボールの予測軌道を集め、TennisReach::FindCandidates を呼ぶだけ。
// 探している間（StartSearching〜StopSearching。予約がバッファ中の間）は毎フレーム探し、見つかったら OnCandidateFound で知らせて止まる。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Reach/TennisReach.h"
#include "TennisReachComponent.generated.h"

class UTennisBallSimComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHitCandidateFound, const FHitCandidate&);

UCLASS(ClassGroup = (Tennis))
class TENNIS_API UTennisReachComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTennisReachComponent();

	/** 探索の値（DATA-02 §4.1） */
	UPROPERTY(EditAnywhere, Category = "Tennis")
	FReachParams Params;

	/** 探すボール。ボールより後に処理する（ARCH-01 §5） */
	void SetBall(UTennisBallSimComponent* InBall);
	const UTennisBallSimComponent* GetBall() const;

	/** 探し始める。すぐに 1 回探し、見つからなければ毎フレーム探す。見つかったら OnCandidateFound を呼んで止まる */
	void StartSearching();
	void StopSearching();
	bool IsSearching() const { return bSearching; }

	/** 今の状態で探し、いちばん良い候補を Out に入れる。ボール・コートがない、または候補がなければ false */
	bool FindBest(FHitCandidate& Out);

	/**
	 * 今の選手とボールの状態から、探索の問い合わせ（位置・速度・側・今のボールの時刻など：DD-03 §4.1）を作る。探索と、ストロークが決まった後の
	 * 打点の選び直し（§6.2）が同じものを使う。選手・移動・コート・ボールのどれかがなければ false
	 */
	bool MakeQuery(FReachQuery& Out) const;

	FOnHitCandidateFound OnCandidateFound;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void TrySearch();

	TWeakObjectPtr<UTennisBallSimComponent> Ball;
	TArray<FHitCandidate> Candidates;  // 毎回確保しないため
	bool bSearching = false;
};
