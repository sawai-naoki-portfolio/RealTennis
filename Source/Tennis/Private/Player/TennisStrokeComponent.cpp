#include "Player/TennisStrokeComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Ball/TennisBallSimComponent.h"
#include "Chooser.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/TennisPlayerSettings.h"
#include "Core/TennisDebugSettings.h"
#include "Court/TennisCourt.h"
#include "Data/StrokeData.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisMovementComponent.h"
#include "Player/TennisPlayerAppearance.h"
#include "Player/TennisReachComponent.h"
#include "Player/TennisStrokeSelection.h"
#include "Stroke/TennisStrokeImpact.h"
#include "TennisConstants.h"

#include <limits>
#include "TennisSimLog.h"

namespace
{
	/** ワールドの向き WorldForward を、コート局所のヨー（rad）にする */
	double CourtYaw(const FTransform& Court, const FVector& WorldForward)
	{
		const FVector Local = Court.InverseTransformVectorNoScale(WorldForward);
		return FMath::Atan2(Local.Y, Local.X);
	}

	UAnimInstance* AnimOf(const ATennisCharacter* Character)
	{
		return Character != nullptr && Character->GetMesh() != nullptr ? Character->GetMesh()->GetAnimInstance() : nullptr;
	}
}

UTennisStrokeComponent::UTennisStrokeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;  // 振っている間だけ。ボールの後に処理する（ATennisCharacter::FollowBall）
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

ATennisCharacter* UTennisStrokeComponent::GetCharacter() const
{
	return Cast<ATennisCharacter>(GetOwner());
}

const UTennisBallSimComponent* UTennisStrokeComponent::GetBall() const
{
	const ATennisCharacter* Character = GetCharacter();
	return Character != nullptr && Character->GetReach() != nullptr ? Character->GetReach()->GetBall() : nullptr;
}

bool UTennisStrokeComponent::PlayStroke(const FHitCandidate& Candidate, ETennisShotType ShotType)
{
	ATennisCharacter* Character = GetCharacter();
	UTennisReachComponent* Reach = Character != nullptr ? Character->GetReach() : nullptr;
	UTennisMovementComponent* Movement = Character != nullptr ? Character->GetTennisMovement() : nullptr;
	const UTennisBallSimComponent* Ball = GetBall();
	UAnimInstance* Anim = AnimOf(Character);
	FReachQuery Q;
	if (Movement == nullptr || Movement->GetCourt() == nullptr || Ball == nullptr || Anim == nullptr || !Reach->MakeQuery(Q))
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Stroke] %s: no court, ball or anim instance (not started)"), *GetNameSafe(GetOwner()));
		return false;
	}

	FTennisStrokeChooserInput Input = TennisStrokeSelection::MakeInput(Candidate, ShotType, Q.Backhand, Q.Now);
	UStrokeData* Selected = TennisStrokeSelection::Select(GetDefault<UTennisPlayerSettings>()->StrokeChooser.LoadSynchronous(), Input);
	if (Selected == nullptr || Selected->Montage == nullptr)
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Stroke] %s: no stroke with a montage for this candidate (not started)"), *GetNameSafe(GetOwner()));
		return false;
	}
	// スイートスポットは体ごとに焼いた値（DD-03 §7）。なければ振らない（ほかの体の値でずれを隠さない）
	const UTennisPlayerAppearance* Appearance = Character->GetAppearance();
	const FTennisStrokeFit* Fit = Appearance != nullptr ? Appearance->FindStrokeFit(Selected) : nullptr;
	if (Fit == nullptr)
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Stroke] %s: no baked sweet spot stroke=%s appearance=%s (not started; run bake_player_appearances)"),
			*GetNameSafe(GetOwner()), *Selected->GetName(), *GetNameSafe(Appearance));
		return false;
	}

	// 打点の選び直し（§6.2）と、インパクトの体の向き（§6.2。M3 の狙いはニュートラル：§6.4）
	const FVector2D Aim = TennisStrokeTiming::NeutralAim(Q.SideSign, Sync);
	const FHitCandidate Refined = TennisReach::RefineForStroke(Ball->GetPrediction(), Candidate, Fit->SweetSpotOffset.GetLocation(), Aim, Q,
		Movement->Params, Reach->Params, Sync);
	const double Available = Refined.Time - Q.Now;
	const double StartYaw = CourtYaw(Movement->GetCourt()->GetActorTransform(), Character->GetActorForwardVector());
	const double TargetYaw = TennisStrokeTiming::FacingYaw(FVector2D(Refined.BallPos.X, Refined.BallPos.Y), Aim, Q.SideSign, Sync);
	FStrokeStepper Next;
	if (!TennisStrokeStep::Begin(Next, FStrokeClip{ Selected->ImpactTime, Selected->ImpactTime + Selected->FollowThroughLock }, Available, StartYaw,
		TargetYaw, Sync))
	{
		return false;  // Begin が警告を出した（焼いた時刻がおかしい）
	}

	// 再生できてから、前のストロークを片づける（再生できなければ、前のものはそのまま）。前のモンタージュは Montage_Play が止める
	// （同じ Slot のグループ）。ここで Montage_Stop を呼ぶと、同じアセットなら新しい再生を止めてしまう
	UAnimMontage* Previous = IsStroking() && Stroke != nullptr ? Stroke->Montage.Get() : nullptr;
	if (Anim->Montage_Play(Selected->Montage, 1.0f) <= 0.0f)
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Stroke] %s: could not play %s (not started)"), *GetNameSafe(GetOwner()), *Selected->Montage->GetName());
		return false;
	}
	Anim->Montage_Pause(Selected->Montage);  // 位置はステッパが決める（アニメの更新では進めない：§6.6）
	if (IsStroking())
	{
		UE_LOG(LogTennisStroke, Log, TEXT("[Stroke] %s: a new stroke cuts the previous one state=%s"), *GetNameSafe(GetOwner()),
			Stepper.State == EStrokeState::FollowThrough ? TEXT("follow-through") : TEXT("before-impact"));
		Movement->SetFollowThrough(false);
		if (Previous != nullptr && Previous != Selected->Montage)
		{
			Anim->Montage_Stop(Previous->GetDefaultBlendOutTime(), Previous);  // 別の Slot のグループなら、Montage_Play は止めない
		}
	}

	Stroke = Selected;
	Stepper = Next;
	bReturningToNet = false;
	Hit = Refined;
	ImpactOffset = TennisStrokeImpact::HandedOffset(Fit->SweetSpotOffset.GetLocation(), Q.Handedness);
	LastStepCount = Ball->GetStepCount();
	NetYaw = TennisStrokeTiming::NetYaw(Q.SideSign);
	Movement->SetAutoMoveTarget(Hit.BodyTarget, Available, Reach->Params.FullMargin);  // 選び直した打点に入る位置へ（§6.2）
	ApplyToBody();
	SetComponentTickEnabled(true);
	UE_LOG(LogTennisStroke, Log, TEXT("[Stroke] %s: %s hit=%.4f (candidate %.4f) available=%.3f start=%.3f rate=%.3f delay=%.3f"),
		*GetNameSafe(GetOwner()), *Stroke->GetName(), Hit.Time, Candidate.Time, Available, Stepper.StartPosition, Stepper.PlayRate, Stepper.Delay);
	return true;
}

bool UTennisStrokeComponent::CancelStroke()
{
	if (!TennisStrokeStep::Cancel(Stepper))
	{
		return false;  // 振っていない・インパクトの後（終わりまで進める：§6.6）
	}
	StopStroke();
	bReturningToNet = true;  // 体の向きをネットへ戻す（DD-03 §6.5）
	SetComponentTickEnabled(true);
	UE_LOG(LogTennisStroke, Log, TEXT("[Stroke] %s: canceled before impact"), *GetNameSafe(GetOwner()));
	return true;
}

void UTennisStrokeComponent::StopStroke()
{
	Stepper.State = EStrokeState::None;  // フォロースルーの途中でも打ち切る（知らせない）
	SetComponentTickEnabled(false);
	ATennisCharacter* Character = GetCharacter();
	if (UTennisMovementComponent* Movement = Character != nullptr ? Character->GetTennisMovement() : nullptr)
	{
		Movement->SetFollowThrough(false);
	}
	UAnimInstance* Anim = AnimOf(Character);
	if (Anim != nullptr && Stroke != nullptr)
	{
		Anim->Montage_Stop(Stroke->Montage->GetDefaultBlendOutTime(), Stroke->Montage);
	}
}

void UTennisStrokeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UTennisBallSimComponent* Ball = GetBall();
	ATennisCharacter* Character = GetCharacter();
	UTennisMovementComponent* Movement = Character != nullptr ? Character->GetTennisMovement() : nullptr;
	if (!IsStroking())
	{
		if (!(bReturningToNet && TurnBackToNet(DeltaTime)))
		{
			bReturningToNet = false;
			SetComponentTickEnabled(false);
		}
		return;
	}
	if (Ball == nullptr || Movement == nullptr)
	{
		// 振っている途中にボール・移動がなくなった：打ち切り（制限も外す）、終わりを知らせる（入力を待機に戻す）
		UE_LOG(LogTennisStroke, Warning, TEXT("[Stroke] %s: lost the ball or movement while stroking (stopped)"), *GetNameSafe(GetOwner()));
		StopStroke();
		OnFinished.Broadcast();
		return;
	}

	// ボールのステップの通し番号の差だけ進める（§6.1 の時計）。打点までの残りは、進めた後のボールの時刻から
	const int64 Count = Ball->GetStepCount();
	const int32 NumSteps = static_cast<int32>(FMath::Clamp<int64>(Count - LastStepCount, 0, MAX_int32));
	LastStepCount = Count;
	const double BallTime = Ball->GetState().Time;
	const FStrokeStepEvents Events = TennisStrokeStep::Advance(Stepper, NumSteps, Hit.Time - BallTime, NetYaw, Sync);
	// T のスイートスポット（§6.4 の 5）。選手の移動はストロークの後に動くので、今の位置と速度はこのフレームの始まりのもの。
	// 体の向きは TargetYaw（ステッパはインパクトでちょうどそこに着く）
	FStrokeImpact Impact;
	Impact.Time = BallTime - Events.ImpactLead;
	if (Events.bImpact)
	{
		if (const ATennisCourt* Court = Movement->GetCourt())
		{
			const FTransform& CourtTransform = Court->GetActorTransform();
			const FVector Start = ATennisCourt::WorldToLocalMeters(CourtTransform, Character->GetActorLocation());
			const FVector Velocity = ATennisCourt::WorldToLocalMetersVector(CourtTransform, Movement->Velocity);
			const FVector2D Body = TennisStrokeImpact::BodyAt(FVector2D(Start.X, Start.Y), FVector2D(Velocity.X, Velocity.Y),
				BallTime - NumSteps * TennisConst::SimDt, Impact.Time);
			Impact.SweetSpot = TennisStrokeImpact::SweetSpotAt(Body, Stepper.TargetYaw, ImpactOffset);
		}
		else
		{
			// コートがない：位置が分からないので、届かない位置にする（判定は空振り。原点との距離で打たない）
			UE_LOG(LogTennisStroke, Warning, TEXT("[Stroke] %s: no court at the impact t=%.5f (not hit)"), *GetNameSafe(GetOwner()), Impact.Time);
			Impact.SweetSpot = FVector(std::numeric_limits<double>::quiet_NaN());
		}
	}
	ApplyToBody();

	// 状態をすべて直してから知らせる（受けた側がこのコンポーネントを呼び返すことがある）
	if (Events.bImpact)
	{
		Movement->ClearAutoMoveTarget();
		Movement->SetFollowThrough(true);
		UE_LOG(LogTennisStroke, Log, TEXT("[Stroke] %s: impact t=%.5f hit=%.5f"), *GetNameSafe(GetOwner()), Impact.Time, Hit.Time);
	}
	if (Events.bEnded)
	{
		Movement->SetFollowThrough(false);
		SetComponentTickEnabled(false);
		if (UAnimInstance* Anim = AnimOf(Character))
		{
			Anim->Montage_Resume(Stroke->Montage);  // 構えに戻るところまで再生する（§6.6）
		}
		UE_LOG(LogTennisStroke, Log, TEXT("[Stroke] %s: recover start t=%.5f"), *GetNameSafe(GetOwner()), BallTime);
	}
	if (Events.bImpact)
	{
		OnImpact.Broadcast(Impact);
	}
	if (Events.bEnded && !IsStroking())  // インパクトの知らせを受けた側が次のストロークを始めていたら、前のものの終わりは知らせない
	{
		OnFinished.Broadcast();
	}
}

const ATennisCourt* UTennisStrokeComponent::GetCourt() const
{
	const ATennisCharacter* Character = GetCharacter();
	const UTennisMovementComponent* Movement = Character != nullptr ? Character->GetTennisMovement() : nullptr;
	return Movement != nullptr ? Movement->GetCourt() : nullptr;
}

void UTennisStrokeComponent::ApplyToBody() const
{
	ATennisCharacter* Character = GetCharacter();
	if (GetCourt() == nullptr || Stroke == nullptr)
	{
		return;
	}
	if (UAnimInstance* Anim = AnimOf(Character))
	{
		Anim->Montage_SetPosition(Stroke->Montage, static_cast<float>(Stepper.Position));
	}
	SetBodyYaw(Stepper.Yaw);
}

void UTennisStrokeComponent::SetBodyYaw(double Yaw) const
{
	ATennisCharacter* Character = GetCharacter();
	const ATennisCourt* Court = GetCourt();
	if (Court == nullptr)
	{
		return;
	}
	double Sin = 0.0;
	double Cos = 1.0;
	FMath::SinCos(&Sin, &Cos, Yaw);
	const FVector Forward = Court->GetActorTransform().TransformVectorNoScale(FVector(Cos, Sin, 0.0));
	Character->SetActorRotation(FRotator(0.0, Forward.Rotation().Yaw, 0.0));
}

bool UTennisStrokeComponent::TurnBackToNet(float DeltaTime)
{
	const ATennisCharacter* Character = GetCharacter();
	const ATennisCourt* Court = GetCourt();
	if (Court == nullptr || !(Sync.ReturnYawRateDeg > 0.0))
	{
		return false;  // 戻せない：止める（Tick を続けない）
	}
	const double Yaw = CourtYaw(Court->GetActorTransform(), Character->GetActorForwardVector());
	// 見た目の向きだけ（インパクトの向きは TargetYaw で決まる）なので、フレームの時間で進めてよい
	const double Step = static_cast<double>(DeltaTime) * GetDefault<UTennisDebugSettings>()->GetTimeScale();
	const double Next = TennisStrokeTiming::ReturnYaw(Yaw, NetYaw, FMath::DegreesToRadians(Sync.ReturnYawRateDeg), Step);
	SetBodyYaw(Next);
	return FMath::Abs(TennisStrokeTiming::WrapAngle(Next - NetYaw)) > 1e-9;
}
