#include "Player/TennisReachComponent.h"

#include "Ball/TennisBallSimComponent.h"
#include "Court/TennisCourt.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisMovementComponent.h"
#include "TennisSimLog.h"

UTennisReachComponent::UTennisReachComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;  // 探している間だけ
}

void UTennisReachComponent::SetBall(UTennisBallSimComponent* InBall)
{
	Ball = InBall;
	if (InBall != nullptr)
	{
		AddTickPrerequisiteComponent(InBall);
	}
}

const UTennisBallSimComponent* UTennisReachComponent::GetBall() const
{
	return Ball.Get();
}

void UTennisReachComponent::StartSearching()
{
	if (bSearching)
	{
		return;
	}
	bSearching = true;
	SetComponentTickEnabled(true);
	TrySearch();
}

void UTennisReachComponent::StopSearching()
{
	bSearching = false;
	SetComponentTickEnabled(false);
}

void UTennisReachComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bSearching)
	{
		TrySearch();
	}
}

void UTennisReachComponent::TrySearch()
{
	FHitCandidate Best;
	if (!FindBest(Best))
	{
		return;
	}
	StopSearching();  // 知らせる前に止める（受け取った側が、続けて意図を渡し直すため）
	UE_LOG(LogTennisStroke, Log, TEXT("[Reach] %s: candidate kind=%s side=%s time=%.3f body=%.2f,%.2f reach=%.3f stretch=%d score=%.3f"),
		*GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Best.Kind), *UEnum::GetValueAsString(Best.Side), Best.Time,
		Best.BodyTarget.X, Best.BodyTarget.Y, Best.TimeToReach, Best.bStretch, Best.Score);
	OnCandidateFound.Broadcast(Best);
}

bool UTennisReachComponent::MakeQuery(FReachQuery& Out) const
{
	const ATennisCharacter* Character = Cast<ATennisCharacter>(GetOwner());
	const UTennisMovementComponent* Movement = Character != nullptr ? Character->GetTennisMovement() : nullptr;
	const ATennisCourt* Court = Movement != nullptr ? Movement->GetCourt() : nullptr;
	const UTennisBallSimComponent* BallSim = Ball.Get();
	if (Court == nullptr || BallSim == nullptr)
	{
		return false;
	}
	const FVector Position = ATennisCourt::WorldToLocalMeters(Court->GetActorTransform(), Character->GetActorLocation());
	Out = FReachQuery();
	Out.Position = FVector2D(Position.X, Position.Y);
	Out.Velocity = Movement->GetMoveState().Velocity;  // モデルの速度（コート局所の m/s）。CMC の速度はフレームの分け方で変わる（DD-03 §2.2）
	Out.BoostTimeLeft = Movement->GetMoveState().BoostTimeLeft;
	Out.SideSign = Movement->GetSideSign();
	Out.Now = BallSim->GetState().Time;
	Out.PrepTime = Params.DefaultPrepTime;  // ストロークごとの準備の時間（WindupTime）を焼くまでは既定の値（DD-03 §5.2）
	return true;
}

bool UTennisReachComponent::FindBest(FHitCandidate& Out)
{
	FReachQuery Q;
	if (!MakeQuery(Q))
	{
		return false;
	}
	// MakeQuery が成功したので、選手・移動・ボールはある
	const UTennisBallSimComponent* BallSim = Ball.Get();
	const UTennisMovementComponent* Movement = CastChecked<ATennisCharacter>(GetOwner())->GetTennisMovement();
	TennisReach::FindCandidates(BallSim->GetPrediction(), Q, Movement->Params, Params, Candidates);
	if (Candidates.IsEmpty())
	{
		return false;
	}
	Out = Candidates[0];
	return true;
}
