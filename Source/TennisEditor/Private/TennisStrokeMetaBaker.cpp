#include "TennisStrokeMetaBaker.h"

#include "AnimPose.h"
#include "Animation/AnimMontage.h"
#include "Animation/TennisAnimNotifies.h"
#include "Components/SkeletalMeshComponent.h"
#include "Court/TennisCourt.h"
#include "Data/StrokeData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisPlayerAppearance.h"
#include "Player/TennisRacketComponent.h"
#include "Player/TennisStrokeSelection.h"
#include "TennisSimLog.h"

namespace
{
	constexpr double RootToleranceCm = 0.1;         // ルートの骨がこれより動いていたら、その場のモンタージュでない
	constexpr double RootRotationTolerance = 1e-3;  // 回転（クォータニオンの成分の差）。これより回っていても、その場でない

	/** 理由を入れて false（ログは呼んだ側が 1 回だけ出す） */
	bool Reject(FString& OutError, const FString& Reason)
	{
		OutError = Reason;
		return false;
	}

	/** 焼いた値（書く前） */
	struct FBakeResult
	{
		double ImpactTime = 0.0;
		double RecoverTime = 0.0;
		FTransform SweetSpot;  // 足元の空間（m）
	};

	/** Montage の Notify のうち、クラスが NotifyClass のものの時刻。ちょうど 1 つでなければ false */
	bool FindNotifyTime(const UAnimMontage& Montage, const UClass* NotifyClass, double& OutTime)
	{
		int32 Count = 0;
		for (const FAnimNotifyEvent& Event : Montage.Notifies)
		{
			if (Event.Notify != nullptr && Event.Notify->IsA(NotifyClass))
			{
				OutTime = Event.GetTriggerTime();
				++Count;
			}
		}
		return Count == 1;
	}

	FTransform SocketTransform(const UStaticMeshSocket& Socket)
	{
		return FTransform(Socket.RelativeRotation, Socket.RelativeLocation, Socket.RelativeScale);
	}

	/**
	 * モンタージュの時刻 Time の姿勢。UAnimPoseExtensions はモンタージュを評価できない（UAnimMontage::GetAnimationPose は
	 * check(false)）ので、最初のスロットのトラックから、その時刻のアニメーションと位置を取り出して評価する。
	 */
	bool PoseAt(const UAnimMontage& Montage, double Time, const FAnimPoseEvaluationOptions& Options, FAnimPose& OutPose)
	{
		if (Montage.SlotAnimTracks.IsEmpty())
		{
			return false;
		}
		const float TrackTime = static_cast<float>(Time);
		const FAnimSegment* Segment = Montage.SlotAnimTracks[0].AnimTrack.GetSegmentAtTime(TrackTime);
		const UAnimSequenceBase* Animation = Segment != nullptr ? Segment->GetAnimReference().Get() : nullptr;
		if (Animation == nullptr || Animation->IsA<UAnimMontage>())
		{
			return false;
		}
		UAnimPoseExtensions::GetAnimPoseAtTime(Animation, Segment->ConvertTrackPosToAnimPos(TrackTime), Options, OutPose);
		return UAnimPoseExtensions::IsValid(OutPose);
	}
}

/**
 * 検査をして、時刻を求める。PlayerMesh と RacketMesh があれば、その体でのスイートスポットも求める（何も書かない）。
 * Bake（時刻だけ）と BakeStrokeFit（体ごとのスイートスポット）の共通の計算
 */
static bool Evaluate(const UStrokeData* Stroke, USkeletalMesh* PlayerMesh, const UStaticMesh* RacketMesh, FBakeResult& Out, FString& OutError)
{
	if (Stroke == nullptr || Stroke->Montage == nullptr)
	{
		return Reject(OutError, TEXT("no stroke or montage"));
	}
	const UAnimMontage& Montage = *Stroke->Montage;
	const bool bSweetSpot = PlayerMesh != nullptr && RacketMesh != nullptr;
	const UStaticMeshSocket* Grip = bSweetSpot ? RacketMesh->FindSocket(UTennisRacketComponent::GripSocket) : nullptr;
	const UStaticMeshSocket* SweetSpot = bSweetSpot ? RacketMesh->FindSocket(UTennisRacketComponent::SweetSpotSocket) : nullptr;
	if (bSweetSpot && (Grip == nullptr || SweetSpot == nullptr || PlayerMesh->FindSocket(ATennisCharacter::RacketHandSocket) == nullptr))
	{
		return Reject(OutError, TEXT("missing socket (racket: grip, racket_sweetspot; player: racket_attach_r)"));
	}
	if (!FMath::IsNearlyEqual(Montage.RateScale, 1.0f))
	{
		return Reject(OutError, FString::Printf(TEXT("montage RateScale is %.3f (must be 1)"), Montage.RateScale));
	}

	double ImpactTime = 0.0;
	double RecoverTime = 0.0;
	if (!FindNotifyTime(Montage, UAnimNotify_TennisImpact::StaticClass(), ImpactTime)
		|| !FindNotifyTime(Montage, UAnimNotify_TennisRecoverStart::StaticClass(), RecoverTime))
	{
		return Reject(OutError, TEXT("the montage needs exactly one Tennis Impact and one Tennis Recover Start notify"));
	}
	const double Length = Montage.GetPlayLength();
	if (!(0.0 < ImpactTime && ImpactTime < RecoverTime && RecoverTime <= Length))
	{
		return Reject(OutError, FString::Printf(TEXT("notify order: impact %.3f, recover %.3f, length %.3f"), ImpactTime, RecoverTime, Length));
	}

	// ルートの骨は骨格の 0 番（名前を決め打ちしない。名前の違う骨を引くと、エンジンは単位の変換を返し、検査が素通りになる）。
	// 体がなければ、モンタージュの骨格で評価する
	const USkeleton* Skeleton = Montage.GetSkeleton();
	const FReferenceSkeleton* Reference = PlayerMesh != nullptr ? &PlayerMesh->GetRefSkeleton() : Skeleton != nullptr ? &Skeleton->GetReferenceSkeleton() : nullptr;
	if (Reference == nullptr || Reference->GetNum() == 0)
	{
		return Reject(OutError, TEXT("the montage has no skeleton"));
	}
	const FName RootBone = Reference->GetBoneName(0);
	FAnimPoseEvaluationOptions Options;
	Options.OptionalSkeletalMesh = PlayerMesh;  // 手のソケット（骨格のソケット）を引くため。なければ null（モンタージュの骨格）
	constexpr int32 ImpactIndex = 1;
	const double Times[] = { 0.0, ImpactTime, Length };
	FTransform HandSocket;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Times); ++Index)
	{
		const double Time = Times[Index];
		FAnimPose Pose;
		if (!PoseAt(Montage, Time, Options, Pose))
		{
			return Reject(OutError, FString::Printf(TEXT("could not evaluate the pose at %.3f s"), Time));
		}
		const FTransform Root = UAnimPoseExtensions::GetBonePose(Pose, RootBone, EAnimPoseSpaces::World);
		if (!Root.GetLocation().IsNearlyZero(RootToleranceCm) || !Root.GetRotation().IsIdentity(RootRotationTolerance))
		{
			return Reject(OutError, FString::Printf(TEXT("the root bone moves at %.3f s (strokes must be in place)"), Time));
		}
		if (bSweetSpot && Index == ImpactIndex)
		{
			HandSocket = UAnimPoseExtensions::GetSocketPose(Pose, ATennisCharacter::RacketHandSocket, EAnimPoseSpaces::World);
		}
	}
	if (!bSweetSpot)
	{
		Out = FBakeResult{ ImpactTime, RecoverTime, FTransform::Identity };
		return true;
	}

	// ラケットは grip を手のソケットに重ねる（UTennisRacketComponent::SetRacketMesh と同じ）。メッシュの空間 → 選手の足元の空間
	const FTransform SweetInMesh = SocketTransform(*SweetSpot) * SocketTransform(*Grip).Inverse() * HandSocket;
	const ATennisCharacter* Character = GetDefault<ATennisCharacter>();
	FTransform SweetAtFeet = SweetInMesh * Character->GetMesh()->GetRelativeTransform();
	SweetAtFeet.AddToTranslation(FVector(0.0, 0.0, ATennisCharacter::CapsuleHalfHeight));  // アクターの原点（カプセルの中心）→ 足元
	SweetAtFeet.SetLocation(ATennisCourt::WorldToLocalMetersVector(FTransform::Identity, SweetAtFeet.GetLocation()));
	SweetAtFeet.SetScale3D(FVector::OneVector);
	Out = FBakeResult{ ImpactTime, RecoverTime, SweetAtFeet };
	return true;
}

bool UTennisStrokeMetaBaker::Bake(UStrokeData* Stroke, FString& OutError)
{
	OutError.Reset();
	FBakeResult Result;
	if (!Evaluate(Stroke, nullptr, nullptr, Result, OutError))
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[StrokeMetaBaker] stroke not baked stroke=%s reason=%s"), *GetNameSafe(Stroke), *OutError);
		return false;
	}
	Stroke->Modify();
	Stroke->ImpactTime = Result.ImpactTime;
	Stroke->FollowThroughLock = Result.RecoverTime - Result.ImpactTime;
	Stroke->MarkPackageDirty();
	UE_LOG(LogTennisStroke, Log, TEXT("[StrokeMetaBaker] stroke baked stroke=%s impact=%.3f lock=%.3f"), *Stroke->GetName(), Result.ImpactTime,
		Stroke->FollowThroughLock);
	return true;
}

bool UTennisStrokeMetaBaker::BakeStrokeFit(const UStrokeData* Stroke, USkeletalMesh* Body, const UStaticMesh* RacketMesh, FTransform& OutSweetSpot,
	FString& OutError)
{
	OutError.Reset();
	FBakeResult Result;
	if (Body == nullptr || RacketMesh == nullptr)
	{
		Reject(OutError, TEXT("no body or racket mesh"));
	}
	else if (Evaluate(Stroke, Body, RacketMesh, Result, OutError))
	{
		OutSweetSpot = Result.SweetSpot;
		return true;
	}
	UE_LOG(LogTennisStroke, Warning, TEXT("[StrokeMetaBaker] sweet spot not baked stroke=%s body=%s reason=%s"), *GetNameSafe(Stroke),
		*GetNameSafe(Body), *OutError);
	return false;
}

bool UTennisStrokeMetaBaker::BakeAppearance(UTennisPlayerAppearance* Appearance, const TArray<UStrokeData*>& Strokes, const UChooserTable* StrokeChooser,
	UStaticMesh* RacketMesh, FString& OutError)
{
	OutError.Reset();
	auto Fail = [Appearance, &OutError](const FString& Reason)  // 1 回だけログを出す（ARCH-03 §5.2.1）
	{
		OutError = Reason;
		UE_LOG(LogTennisStroke, Warning, TEXT("[StrokeMetaBaker] appearance not baked appearance=%s reason=%s"), *GetNameSafe(Appearance), *Reason);
		return false;
	};
	USkeletalMesh* Body = Appearance != nullptr ? Appearance->Body.LoadSynchronous() : nullptr;
	TArray<UStrokeData*> Sorted;
	for (UStrokeData* Stroke : Strokes)
	{
		if (Stroke != nullptr)
		{
			Sorted.AddUnique(Stroke);
		}
	}
	if (Body == nullptr || StrokeChooser == nullptr || RacketMesh == nullptr || Sorted.IsEmpty())
	{
		return Fail(TEXT("no appearance, body, stroke chooser, racket mesh or strokes"));
	}
	Sorted.Sort([](const UStrokeData& A, const UStrokeData& B) { return A.GetPathName() < B.GetPathName(); });  // 名前の順（決定論）

	TArray<FTennisStrokeFit> Fits;
	for (UStrokeData* Stroke : Sorted)
	{
		FBakeResult Result;
		FString Error;
		if (!Evaluate(Stroke, Body, RacketMesh, Result, Error))  // BakeStrokeFit と同じ計算。ログはここで 1 回だけ
		{
			return Fail(FString::Printf(TEXT("stroke=%s: %s"), *Stroke->GetName(), *Error));
		}
		Fits.Add(FTennisStrokeFit{ TSoftObjectPtr<UStrokeData>(Stroke), Result.SweetSpot });
	}

	// 探索の体の位置（DD-03 §4.2）：中くらいの高さのドライブで選ばれるストローク（T-STRK-011 と同じ入力。右利き：フォアは +Y）
	auto SweetSpotOf = [&Fits, StrokeChooser](EStrokeSide Side, EBackhandStyle Style, FVector& OutLocation)
	{
		FTennisStrokeChooserInput Input = TennisStrokeSelection::MidDriveInput(Side, Style);
		const FTennisStrokeFit* Fit = FTennisStrokeFit::Find(Fits, TennisStrokeSelection::Select(StrokeChooser, Input));
		OutLocation = Fit != nullptr ? Fit->SweetSpotOffset.GetLocation() : FVector::ZeroVector;
		return Fit != nullptr;
	};
	FVector Forehand, OneHanded, TwoHanded;
	if (!SweetSpotOf(EStrokeSide::Forehand, EBackhandStyle::OneHanded, Forehand) || !SweetSpotOf(EStrokeSide::Backhand, EBackhandStyle::OneHanded, OneHanded)
		|| !SweetSpotOf(EStrokeSide::Backhand, EBackhandStyle::TwoHanded, TwoHanded))
	{
		return Fail(TEXT("the stroke chooser selects no baked stroke for a mid-height drive"));
	}
	FTennisReachBody Reach;
	Reach.Forward = (Forehand.X + OneHanded.X + TwoHanded.X) / 3.0;
	Reach.ForehandSide = Forehand.Y;
	Reach.OneHandedBackhandSide = -OneHanded.Y;
	Reach.TwoHandedBackhandSide = -TwoHanded.Y;
	if (!Reach.IsBaked())
	{
		return Fail(FString::Printf(TEXT("the reach body is not in front and to the sides: forward=%.3f forehand=%.3f backhand1=%.3f backhand2=%.3f"),
			Reach.Forward, Reach.ForehandSide, Reach.OneHandedBackhandSide, Reach.TwoHandedBackhandSide));
	}

	Appearance->Modify();
	Appearance->StrokeFits = MoveTemp(Fits);
	Appearance->ReachBody = Reach;
	Appearance->MarkPackageDirty();
	UE_LOG(LogTennisStroke, Log, TEXT("[StrokeMetaBaker] appearance baked appearance=%s strokes=%d body=%s forward=%.3f forehand=%.3f backhand1=%.3f "
		"backhand2=%.3f"), *Appearance->GetName(), Appearance->StrokeFits.Num(), *Body->GetName(), Reach.Forward, Reach.ForehandSide,
		Reach.OneHandedBackhandSide, Reach.TwoHandedBackhandSide);
	return true;
}
