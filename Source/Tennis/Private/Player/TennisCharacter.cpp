#include "Player/TennisCharacter.h"

#include "Ball/TennisBall.h"
#include "Animation/AnimInstance.h"
#include "Ball/TennisBallSimComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/TennisPlayerSettings.h"
#include "Data/ShotTypeData.h"
#include "Data/StrokeData.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "GroomComponent.h"
#include "Player/TennisMovementComponent.h"
#include "Player/TennisPlayerAppearance.h"
#include "Player/TennisRacketComponent.h"
#include "Player/TennisReachComponent.h"
#include "Player/TennisStrokeComponent.h"
#include "Shot/ShotRequestMapping.h"
#include "Shot/TennisShotSolver.h"
#include "Stroke/TennisStrokeImpact.h"
#include "Stroke/TennisStrokeTiming.h"
#include "TennisGameLog.h"
#include "TennisSimLog.h"

const FName ATennisCharacter::RacketHandSocket(TEXT("racket_attach_r"));

ATennisCharacter::ATennisCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTennisMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;  // 動きは移動のコンポーネントが進める
	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	bUseControllerRotationYaw = false;  // 体の向きはコントローラーの向きに合わせない（DD-03 §2.2）
	Reach = CreateDefaultSubobject<UTennisReachComponent>(TEXT("Reach"));
	Stroke = CreateDefaultSubobject<UTennisStrokeComponent>(TEXT("Stroke"));

	// メッシュの原点（足元）をカプセルの底に、メッシュの前（+Y）をキャラクターの前（+X）に合わせる
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0, 0.0, -CapsuleHalfHeight), FRotator(0.0, -90.0, 0.0));
	Racket = CreateDefaultSubobject<UTennisRacketComponent>(TEXT("Racket"));
	Racket->SetupAttachment(GetMesh());  // 手のソケットへは、メッシュを決めてから付ける（ApplyAppearance）
}

void ATennisCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	Reach->OnCandidateFound.AddUObject(this, &ATennisCharacter::HandleCandidateFound);
	Stroke->OnImpact.AddUObject(this, &ATennisCharacter::HandleStrokeImpact);
	Stroke->OnFinished.AddUObject(this, &ATennisCharacter::HandleStrokeFinished);
	ApplyAppearance(GetDefault<UTennisPlayerSettings>()->Appearance.LoadSynchronous());
}

bool ATennisCharacter::ApplyAppearance(const UTennisPlayerAppearance* NewAppearance)
{
	for (UGroomComponent* Old : Grooms)  // 髪は部品に付いているので、部品より先に消す
	{
		if (Old != nullptr)
		{
			Old->DestroyComponent();
		}
	}
	Grooms.Reset();
	for (USkeletalMeshComponent* Old : Followers)
	{
		if (Old != nullptr)
		{
			Old->DestroyComponent();
		}
	}
	Followers.Reset();
	Appearance = nullptr;

	USkeletalMesh* Body = NewAppearance != nullptr ? NewAppearance->Body.LoadSynchronous() : nullptr;
	if (Body == nullptr)
	{
		UE_LOG(LogTennisMove, Warning, TEXT("[Player] %s: no body mesh in the appearance %s (showing the capsule)"),
			*GetName(), *GetNameSafe(NewAppearance));
		GetCapsuleComponent()->SetHiddenInGame(false);
		Racket->SetHiddenInGame(true);
		ApplyReachBody(nullptr);  // 前の体の値を残さない
		return false;
	}
	Appearance = NewAppearance;
	GetMesh()->SetSkeletalMesh(Body);
	ApplyReachBody(Appearance);
	GetCapsuleComponent()->SetHiddenInGame(true);
	Racket->SetHiddenInGame(false);
	Racket->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, RacketHandSocket);
	const UTennisPlayerSettings* Settings = GetDefault<UTennisPlayerSettings>();
	if (UClass* AnimClass = Settings->AnimClass.LoadSynchronous())
	{
		GetMesh()->SetAnimInstanceClass(AnimClass);
	}
	else
	{
		UE_LOG(LogTennisMove, Warning, TEXT("[Player] %s: no AnimClass in Project Settings > Tennis Player (no animation)"), *GetName());
	}
	Racket->SetRacketMesh(Settings->RacketMesh.LoadSynchronous());

	for (int32 Index = 0; Index < Appearance->Followers.Num(); ++Index)
	{
		USkeletalMesh* Part = Appearance->Followers[Index].LoadSynchronous();
		if (Part == nullptr)
		{
			UE_LOG(LogTennisMove, Warning, TEXT("[Player] %s: follower %d of %s has no mesh (skipped)"), *GetName(), Index, *GetNameSafe(Appearance));
			continue;
		}
		USkeletalMeshComponent* Follower = NewObject<USkeletalMeshComponent>(this, MakeUniqueObjectName(this, USkeletalMeshComponent::StaticClass(), TEXT("Follower")));
		Follower->SetSkeletalMesh(Part);
		Follower->SetupAttachment(GetMesh());
		Follower->SetLeaderPoseComponent(GetMesh());  // 体の骨の姿勢をそのまま使う（DD-03 §7）
		Follower->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AddInstanceComponent(Follower);
		if (GetWorld() != nullptr)
		{
			Follower->RegisterComponent();
		}
		Followers.Add(Follower);
	}
	AttachGrooms(*Appearance);
	return true;
}

void ATennisCharacter::ApplyReachBody(const UTennisPlayerAppearance* Baked)
{
	FTennisReachBody Body;
	if (Baked != nullptr && Baked->ReachBody.IsBaked())
	{
		Body = Baked->ReachBody;
	}
	else
	{
		const FReachParams Defaults;  // パラメータ表の既定（DATA-02 §4.1。仮の男性の焼いた値）
		Body.Forward = Defaults.Forward;
		Body.ForehandSide = Defaults.ForehandSide;
		Body.OneHandedBackhandSide = Defaults.OneHandedBackhandSide;
		Body.TwoHandedBackhandSide = Defaults.TwoHandedBackhandSide;
		if (Baked != nullptr)
		{
			UE_LOG(LogTennisMove, Warning, TEXT("[Player] %s: reach body not baked appearance=%s (run bake_player_appearances; using the defaults)"),
				*GetName(), *GetNameSafe(Baked));
		}
	}
	FReachParams& Params = Reach->Params;  // 探索の体の位置は体で決まる（DD-03 §4.2）
	Params.Forward = Body.Forward;
	Params.ForehandSide = Body.ForehandSide;
	Params.OneHandedBackhandSide = Body.OneHandedBackhandSide;
	Params.TwoHandedBackhandSide = Body.TwoHandedBackhandSide;
}

void ATennisCharacter::AttachGrooms(const UTennisPlayerAppearance& Source)
{
	for (int32 Index = 0; Index < Source.GroomBindings.Num(); ++Index)
	{
		UGroomBindingAsset* Binding = Source.GroomBindings[Index].LoadSynchronous();
		UGroomAsset* Asset = Binding != nullptr ? Binding->GetGroom() : nullptr;
		if (Asset == nullptr)
		{
			UE_LOG(LogTennisMove, Warning, TEXT("[Player] %s: groom skipped appearance=%s index=%d binding=%s groom=%s"), *GetName(),
				*GetNameSafe(&Source), Index, *GetNameSafe(Binding), *GetNameSafe(Asset));
			continue;
		}

		// Binding の対象のメッシュを持つ部品（MetaHuman の髪は顔）。追従する部品を先に見る
		const USkeletalMesh* Target = Binding->GetTargetSkeletalMesh();
		USkeletalMeshComponent* Parent = nullptr;
		for (USkeletalMeshComponent* Follower : Followers)
		{
			if (Target != nullptr && Follower->GetSkeletalMeshAsset() == Target)
			{
				Parent = Follower;
				break;
			}
		}
		if (Parent == nullptr)
		{
			Parent = GetMesh();
			if (Target == nullptr || GetMesh()->GetSkeletalMeshAsset() != Target)
			{
				UE_LOG(LogTennisMove, Warning, TEXT("[Player] %s: groom attached to the body (no part has the target) appearance=%s index=%d target=%s"),
					*GetName(), *GetNameSafe(&Source), Index, *GetNameSafe(Target));
			}
		}

		UGroomComponent* Groom = NewObject<UGroomComponent>(this, MakeUniqueObjectName(this, UGroomComponent::StaticClass(), TEXT("Groom")));
		Groom->SetupAttachment(Parent);
		Groom->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Groom->SetGroomAsset(Asset, Binding);  // 部品の姿勢で動く（Binding の対象のメッシュ）
		AddInstanceComponent(Groom);
		if (GetWorld() != nullptr)
		{
			Groom->RegisterComponent();
		}
		Grooms.Add(Groom);
	}
}

UTennisMovementComponent* ATennisCharacter::GetTennisMovement() const
{
	return Cast<UTennisMovementComponent>(GetCharacterMovement());
}

void ATennisCharacter::BeginPlay()
{
	Super::BeginPlay();
	for (TActorIterator<ATennisBall> It(GetWorld()); It; ++It)
	{
		if (UTennisBallSimComponent* Ball = It->GetSim())
		{
			FollowBall(*Ball);
			return;
		}
	}
	UE_LOG(LogTennisMove, Warning, TEXT("[Player] %s: no ball in the level (tick order and hit search are not set)"), *GetName());
}

void ATennisCharacter::FollowBall(UTennisBallSimComponent& Ball)
{
	Stroke->AddTickPrerequisiteComponent(&Ball);
	GetCharacterMovement()->AddTickPrerequisiteComponent(&Ball);
	GetCharacterMovement()->AddTickPrerequisiteComponent(Stroke);
	GetMesh()->AddTickPrerequisiteComponent(&Ball);
	GetMesh()->AddTickPrerequisiteComponent(Stroke);
	Reach->SetBall(&Ball);
	FollowedBall = &Ball;
}

void ATennisCharacter::SubmitShotIntent(const FShotIntent& Intent)
{
	ShotIntent = Intent;
	if (Intent.ReleaseTime.IsSet())
	{
		// 予約した打点がない（振っていない）ときだけ終える。ストロークの状態では決めない（インパクトと RecoverStart が同じ Tick に来ると、
		// インパクトの知らせの中でストロークはもう終わっている。終わりは OnFinished の 1 回だけにする）
		if (!PlannedHit.IsSet())
		{
			EndStroke(TEXT("released without a planned hit"));
		}
		return;  // 振っていれば、離した時刻を記録するだけ（品質：M4）。ストロークは RecoverStart で終わる（DD-03 §6.6）
	}
	if (!Intent.ChargeStartTime.IsSet() && !PlannedHit.IsSet())
	{
		Reach->StartSearching();  // 見つかれば、この呼び出しの中で HandleCandidateFound が呼ばれる
	}
}

void ATennisCharacter::CancelShotIntent()
{
	ShotIntent.Reset();
	PlannedHit.Reset();
	Reach->StopSearching();
	Stroke->CancelStroke();  // インパクトの前なら止める。後なら終わりまで進む（その終わりは知らせない：HandleStrokeFinished）
	GetTennisMovement()->ClearAutoMoveTarget();
}

const FHitCandidate* ATennisCharacter::GetPlannedHit() const
{
	return PlannedHit.GetPtrOrNull();
}

void ATennisCharacter::HandleCandidateFound(const FHitCandidate& Candidate)
{
	PlannedHit = Candidate;
	// 先に入力へ知らせる（バッファ → 溜め中）。ストロークを始められなかったときに終えると、入力が溜め中から待機に戻る（ST-05）
	OnHitCandidateFound.Broadcast();
	if (!ShotIntent.IsSet())
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Stroke] %s: a hit candidate without a shot intent (using topspin)"), *GetName());
	}
	const ETennisShotType Type = ShotIntent.IsSet() ? ShotIntent->Type : ETennisShotType::Topspin;
	if (!Stroke->PlayStroke(Candidate, Type))  // 選び、打点を選び直し、自動移動の目標を置き、振り始める（DD-03 §6.6）
	{
		EndStroke(TEXT("no stroke for the candidate"));
		return;
	}
	PlannedHit = Stroke->GetPlannedHit();
}

void ATennisCharacter::HandleStrokeImpact(const FStrokeImpact& Impact)
{
	// 意図を先に読む：知らせを受けた入力が意図を取り消すことがある（ST-05）。押したまま（離していない）なら溜めは最大
	if (!ShotIntent.IsSet())
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Stroke] %s: an impact without a shot intent (using topspin and full charge)"), *GetName());
	}
	const ETennisShotType Type = ShotIntent.IsSet() ? ShotIntent->Type : ETennisShotType::Topspin;
	const UStrokeData* Data = Stroke->GetStroke();
	const double Charge = ShotIntent.IsSet() && Data != nullptr ? TennisShotRequestMapping::ChargeAtImpact(*ShotIntent, Data->ChargeTime) : 1.0;
	HitBall(Impact, Type, Charge);
	OnStrokeImpact.Broadcast();  // 空振りでも知らせる（入力は確定へ進む）
}

void ATennisCharacter::HitBall(const FStrokeImpact& Impact, ETennisShotType Type, double Charge)
{
	const UTennisMovementComponent* Movement = GetTennisMovement();
	FBallState AtImpact;
	if (!IsValid(FollowedBall) || Movement == nullptr || !FollowedBall->GetStateAt(Impact.Time, AtImpact))
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Stroke] %s: no ball state at the impact t=%.5f (not hit)"), *GetName(), Impact.Time);
		return;
	}
	const double Distance = FVector::Dist(AtImpact.Pos, Impact.SweetSpot);
	const bool bInReach = TennisStrokeImpact::IsWithinReach(Distance, Stroke->Sync);
	UE_LOG(LogTennisStroke, Log, TEXT("[Stroke] %s: impact distance=%.4f hit=%d t=%.5f"), *GetName(), Distance, bInReach ? 1 : 0, Impact.Time);
	if (!bInReach)
	{
		return;  // 空振り：打ち出さず、ストロークは最後まで振る
	}
	const UShotTypeData* Shot = GetDefault<UTennisPlayerSettings>()->FindShotType(Type);
	if (Shot == nullptr)
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Stroke] %s: no shot type data type=%d (not hit; add it to Project Settings > Tennis Player > Shot Types)"),
			*GetName(), static_cast<int32>(Type));
		return;
	}
	// M3 の打球（§6.4 の 6）：打点は T のボールの位置、狙いはニュートラル、誤差なし。打ち出しの時刻は T（Launch が今まで早送りする）
	const FVector2D Aim = TennisStrokeTiming::NeutralAim(Movement->GetSideSign(), Stroke->Sync);
	const FShotRequest Request = TennisShotRequestMapping::Make(Shot->Params, AtImpact.Pos, FVector(Aim.X, Aim.Y, 0.0), Impact.Time, Charge,
		ETennisHandedness::Right, 0.0);
	const FShotSolveResult Result = FTennisShotSolver(FollowedBall->GetContext()).Solve(Request);
	if (!Result.bSolved)
	{
		UE_LOG(LogTennisShot, Warning, TEXT("[Hit] %s: the solver did not converge fallback=%d residual=%.3f (hitting the nearest solution)"),
			*GetName(), static_cast<int32>(Result.Fallback), Result.ResidualRange);
	}
	FollowedBall->Launch(Result.Launch);
	UE_LOG(LogTennisStroke, Log, TEXT("[Stroke] %s: hit shot=%s charge=%.2f landing=(%.2f, %.2f)"), *GetName(), *Shot->GetName(), Charge,
		Result.PredictedLanding.X, Result.PredictedLanding.Y);
}

void ATennisCharacter::HandleStrokeFinished()
{
	if (!PlannedHit.IsSet())
	{
		// 取り消した後（インパクトの後は終わりまで進む）のストロークの終わり：意図はもう終えている。次の予約（バッファ中の探索）を消さない
		UE_LOG(LogTennisStroke, Log, TEXT("[Stroke] %s: finished after the intent was canceled"), *GetName());
		return;
	}
	EndStroke(TEXT("recover start"));
}

void ATennisCharacter::EndStroke(const TCHAR* Reason)
{
	UE_LOG(LogTennisStroke, Log, TEXT("[Stroke] %s: ended (%s)"), *GetName(), Reason);
	ShotIntent.Reset();
	PlannedHit.Reset();
	Reach->StopSearching();
	GetTennisMovement()->ClearAutoMoveTarget();
	OnStrokeEnded.Broadcast();
}

const FShotIntent* ATennisCharacter::GetShotIntent() const
{
	return ShotIntent.GetPtrOrNull();
}
