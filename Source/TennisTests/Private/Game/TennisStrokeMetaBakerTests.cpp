// W-3.4、T-STRK-010：StrokeMetaBaker（DD-03 §5.2・§5.4・§7、ART-02 §5、FR-STRK-007）。モンタージュの Notify から時刻を、インパクトの姿勢から
// スイートスポットの位置を読む。位置は、選手にその姿勢を取らせて測ったラケットのスイートスポットと合うこと。見た目のデータごとの焼き（BakeAppearance）。
// ベイカー（TennisEditor）はエディタ専用なので、エディタのビルドだけでテストする。
#include "CQTest.h"
#include "Data/StrokeData.h"

#if WITH_EDITOR
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/TennisAnimNotifies.h"
#include "Components/ActorTestSpawner.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/TennisPlayerSettings.h"
#include "Court/TennisCourt.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Player/TennisCharacter.h"
#include "Player/TennisPlayerAppearance.h"
#include "Player/TennisRacketComponent.h"
#include "Player/TennisStrokeSelection.h"
#include "TennisDataAssetTestHelpers.h"
#include "TennisStrokeMetaBaker.h"

TEST_CLASS(TennisStrokeMetaBaker, "Tennis.Game.Stroke")
{
	FActorTestSpawner Spawner;
	UAnimSequenceBase* Idle = nullptr;
	USkeletalMesh* PlayerMesh = nullptr;
	UStaticMesh* RacketMesh = nullptr;
	UAnimMontage* Montage = nullptr;
	UStrokeData* Stroke = nullptr;
	FString Error;

	BEFORE_EACH()
	{
		// テンプレートの待機のアニメーション（その場。ルートは動かない）を 1 本だけ入れたモンタージュを、その場で作る
		const UTennisPlayerSettings* Settings = GetDefault<UTennisPlayerSettings>();
		const UTennisPlayerAppearance* Appearance = Settings->Appearance.LoadSynchronous();
		Idle = LoadObject<UAnimSequenceBase>(nullptr, TEXT("/Game/Tennis/Characters/Animations/Locomotion/Mannequin/MM_Idle.MM_Idle"));  // その場の待機
		PlayerMesh = Appearance != nullptr ? Appearance->Body.LoadSynchronous() : nullptr;
		RacketMesh = Settings->RacketMesh.LoadSynchronous();
		ASSERT_THAT(IsTrue(Idle != nullptr && PlayerMesh != nullptr && RacketMesh != nullptr, TEXT("MM_Idle and Project Settings > Tennis Player")));
		Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Idle, TEXT("DefaultSlot"));
		Stroke = NewObject<UStrokeData>();
		Stroke->Montage = Montage;
		Error.Reset();
	}

	void AddNotify(UClass* NotifyClass, float Time) const
	{
		FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
		Event.Notify = NewObject<UAnimNotify>(Montage, NotifyClass);
		Event.Link(Montage, Time);
	}

	bool Bake() { return UTennisStrokeMetaBaker::Bake(Stroke, Error); }

	// 時刻は Notify から。スイートスポットは、選手にインパクトの時刻の姿勢を取らせて測った位置（足元から、m）と 1 cm 以内で合う
	TEST_METHOD(Bake_ReadsTimesAndTheSweetSpotAtImpact)
	{
		AddNotify(UAnimNotify_TennisImpact::StaticClass(), 0.5f);
		AddNotify(UAnimNotify_TennisRecoverStart::StaticClass(), 0.9f);
		ASSERT_THAT(IsTrue(Bake(), *Error));
		ASSERT_THAT(IsNear(0.5, Stroke->ImpactTime, 1e-4));
		ASSERT_THAT(IsNear(0.4, Stroke->FollowThroughLock, 1e-4));
		FTransform SweetSpot;
		ASSERT_THAT(IsTrue(UTennisStrokeMetaBaker::BakeStrokeFit(Stroke, PlayerMesh, RacketMesh, SweetSpot, Error), *Error));

		ATennisCharacter& Character = Spawner.SpawnActor<ATennisCharacter>();
		USkeletalMeshComponent* Mesh = Character.GetMesh();
		Mesh->PlayAnimation(Idle, false);
		Mesh->SetPosition(0.5f, false);
		Mesh->SetPlayRate(0.0f);
		Mesh->TickAnimation(0.0f, false);
		Mesh->RefreshBoneTransforms();
		Mesh->UpdateChildTransforms();
		const FVector Feet = Character.GetActorLocation() - FVector(0.0, 0.0, ATennisCharacter::CapsuleHalfHeight);
		const FVector Measured = ATennisCourt::WorldToLocalMetersVector(FTransform::Identity,
			Character.GetActorRotation().UnrotateVector(Character.GetRacket()->GetSweetSpotWorld() - Feet));
		const FVector Baked = SweetSpot.GetLocation();
		ASSERT_THAT(IsTrue(Baked.Equals(Measured, 0.01), *FString::Printf(TEXT("baked %s, measured %s"), *Baked.ToString(), *Measured.ToString())));
		ASSERT_THAT(IsTrue(Baked.Z > 0.5 && Baked.Z < 1.6 && Baked.Size2D() < 1.2));  // 手の届く範囲にある
	}

	// Notify が足りない・多い・順序がおかしいモンタージュは焼かない（値を書かない。理由を返す）
	TEST_METHOD(Bake_RejectsMissingOrMisorderedNotifies)
	{
		ASSERT_THAT(IsFalse(Bake()));  // どちらもない
		ASSERT_THAT(IsFalse(Error.IsEmpty()));
		AddNotify(UAnimNotify_TennisImpact::StaticClass(), 0.9f);
		ASSERT_THAT(IsFalse(Bake()));  // RecoverStart がない
		AddNotify(UAnimNotify_TennisRecoverStart::StaticClass(), 0.5f);
		ASSERT_THAT(IsFalse(Bake()));  // インパクトより前に RecoverStart
		AddNotify(UAnimNotify_TennisImpact::StaticClass(), 0.2f);
		ASSERT_THAT(IsFalse(Bake()));  // Impact が 2 つ
		ASSERT_THAT(IsNear(0.0, Stroke->ImpactTime, 1e-9));
	}

	// RecoverStart はモンタージュの末尾でもよい。インパクトと同じ時刻は順序がおかしい
	TEST_METHOD(Bake_AcceptsRecoverAtTheEndButNotAtImpact)
	{
		AddNotify(UAnimNotify_TennisImpact::StaticClass(), 0.5f);
		AddNotify(UAnimNotify_TennisRecoverStart::StaticClass(), Montage->GetPlayLength());
		ASSERT_THAT(IsTrue(Bake(), *Error));
		Montage->Notifies.Last().Link(Montage, 0.5f);
		ASSERT_THAT(IsFalse(Bake()));
	}

	// ラケットにソケット（grip・racket_sweetspot）がなければ焼かない
	TEST_METHOD(Bake_RejectsAMeshWithoutSockets)
	{
		AddNotify(UAnimNotify_TennisImpact::StaticClass(), 0.5f);
		AddNotify(UAnimNotify_TennisRecoverStart::StaticClass(), 0.9f);
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		ASSERT_THAT(IsNotNull(Cube));
		FTransform SweetSpot;
		ASSERT_THAT(IsFalse(UTennisStrokeMetaBaker::BakeStrokeFit(Stroke, PlayerMesh, Cube, SweetSpot, Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("socket")), *Error));
	}

	// ルートの骨が動くアニメーション（その場でない。テンプレートの歩きは 1.5 s で 4.5 m 進む）は焼かない（DD-03 §6.2）
	TEST_METHOD(Bake_RejectsAMontageWhoseRootMoves)
	{
		UAnimSequenceBase* Walk = LoadObject<UAnimSequenceBase>(nullptr,
			TEXT("/Game/Tennis/Characters/Animations/Locomotion/Mannequin/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd"));
		ASSERT_THAT(IsNotNull(Walk));
		Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Walk, TEXT("DefaultSlot"));
		Stroke->Montage = Montage;
		AddNotify(UAnimNotify_TennisImpact::StaticClass(), 0.5f);
		AddNotify(UAnimNotify_TennisRecoverStart::StaticClass(), 0.9f);
		ASSERT_THAT(IsFalse(Bake()));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("root")), *Error));
		ASSERT_THAT(IsNear(0.0, Stroke->ImpactTime, 1e-9));
		FTransform SweetSpot;  // 体の骨格で評価する経路（MetaHuman の体など）でも、ルートの検査で止まる
		ASSERT_THAT(IsFalse(UTennisStrokeMetaBaker::BakeStrokeFit(Stroke, PlayerMesh, RacketMesh, SweetSpot, Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("root")), *Error));
	}

	// RateScale が 1 でないモンタージュ、入力のないものは焼かない
	TEST_METHOD(Bake_RejectsRateScaleAndMissingInputs)
	{
		AddNotify(UAnimNotify_TennisImpact::StaticClass(), 0.5f);
		AddNotify(UAnimNotify_TennisRecoverStart::StaticClass(), 0.9f);
		Montage->RateScale = 1.2f;
		ASSERT_THAT(IsFalse(Bake()));
		Montage->RateScale = 1.0f;
		FTransform SweetSpot;
		ASSERT_THAT(IsFalse(UTennisStrokeMetaBaker::BakeStrokeFit(Stroke, nullptr, RacketMesh, SweetSpot, Error)));
		ASSERT_THAT(IsFalse(UTennisStrokeMetaBaker::BakeStrokeFit(Stroke, PlayerMesh, nullptr, SweetSpot, Error)));
		ASSERT_THAT(IsFalse(UTennisStrokeMetaBaker::Bake(nullptr, Error)));
		Stroke->Montage = nullptr;
		ASSERT_THAT(IsFalse(Bake()));
	}

	// BakeAppearance：体で、すべての DA_Stroke_* のスイートスポットを BakeStrokeFit と同じ計算で焼き（名前の順）、探索の体の位置を、表が中くらいの
	// 高さのドライブで選ぶストロークの値から作る（横はその横の位置、前はその平均。DD-03 §4.2・§7）
	TEST_METHOD(BakeAppearance_UsesTheBakeFormulaForEveryStroke)
	{
		const UChooserTable* Table = GetDefault<UTennisPlayerSettings>()->StrokeChooser.LoadSynchronous();
		TArray<UStrokeData*> Strokes;
		for (const UStrokeData* Data : TennisTest::LoadAllDataAssets<UStrokeData>(TennisTest::StrokesFolder))
		{
			Strokes.Add(const_cast<UStrokeData*>(Data));  // 焼く道具は読むだけ
		}
		ASSERT_THAT(IsTrue(Table != nullptr && Strokes.Num() > 0));
		UTennisPlayerAppearance* Appearance = NewObject<UTennisPlayerAppearance>();
		Appearance->Body = PlayerMesh;
		ASSERT_THAT(IsTrue(UTennisStrokeMetaBaker::BakeAppearance(Appearance, Strokes, Table, RacketMesh, Error), *Error));
		ASSERT_THAT(AreEqual(Strokes.Num(), Appearance->StrokeFits.Num()));
		for (int32 Index = 1; Index < Appearance->StrokeFits.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(Appearance->StrokeFits[Index - 1].Stroke.ToString() < Appearance->StrokeFits[Index].Stroke.ToString()));
		}
		for (UStrokeData* Data : Strokes)
		{
			FTransform SweetSpot;
			ASSERT_THAT(IsTrue(UTennisStrokeMetaBaker::BakeStrokeFit(Data, PlayerMesh, RacketMesh, SweetSpot, Error), *Error));
			const FTennisStrokeFit* Fit = Appearance->FindStrokeFit(Data);
			ASSERT_THAT(IsTrue(Fit != nullptr && Fit->SweetSpotOffset.Equals(SweetSpot, 1e-9), *Data->GetName()));
		}

		auto MidDrive = [&](EStrokeSide Side, EBackhandStyle Style)
		{
			FTennisStrokeChooserInput Input = TennisStrokeSelection::MidDriveInput(Side, Style);
			const FTennisStrokeFit* Fit = Appearance->FindStrokeFit(TennisStrokeSelection::Select(Table, Input));
			return Fit != nullptr ? Fit->SweetSpotOffset.GetLocation() : FVector::ZeroVector;  // ないと下の比べ合わせで落ちる
		};
		const FVector Forehand = MidDrive(EStrokeSide::Forehand, EBackhandStyle::OneHanded);
		const FVector OneHanded = MidDrive(EStrokeSide::Backhand, EBackhandStyle::OneHanded);
		const FVector TwoHanded = MidDrive(EStrokeSide::Backhand, EBackhandStyle::TwoHanded);
		const FTennisReachBody& Reach = Appearance->ReachBody;
		ASSERT_THAT(IsTrue(Reach.IsBaked()));
		ASSERT_THAT(IsNear(Forehand.Y, Reach.ForehandSide, 1e-9));
		ASSERT_THAT(IsNear(-OneHanded.Y, Reach.OneHandedBackhandSide, 1e-9));
		ASSERT_THAT(IsNear(-TwoHanded.Y, Reach.TwoHandedBackhandSide, 1e-9));
		ASSERT_THAT(IsNear((Forehand.X + OneHanded.X + TwoHanded.X) / 3.0, Reach.Forward, 1e-9));
	}

	// BakeAppearance：入力がない、どれかのストロークが焼けない（Notify のないモンタージュ）、表が中くらいの高さのドライブで焼いた
	// ストロークを選ばない（フォアだけを渡すと、バックで選ばれるストロークがない）なら、何も書かずに false
	TEST_METHOD(BakeAppearance_RejectsBadInputsWithoutWriting)
	{
		const UChooserTable* Table = GetDefault<UTennisPlayerSettings>()->StrokeChooser.LoadSynchronous();
		ASSERT_THAT(IsTrue(Table != nullptr));
		UTennisPlayerAppearance* Appearance = NewObject<UTennisPlayerAppearance>();
		Appearance->Body = PlayerMesh;
		FTennisStrokeChooserInput ForehandInput = TennisStrokeSelection::MidDriveInput(EStrokeSide::Forehand, EBackhandStyle::OneHanded);
		UStrokeData* ForehandOnly = TennisStrokeSelection::Select(Table, ForehandInput);
		ASSERT_THAT(IsTrue(ForehandOnly != nullptr));
		ASSERT_THAT(IsFalse(UTennisStrokeMetaBaker::BakeAppearance(Appearance, {ForehandOnly}, Table, RacketMesh, Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("selects no baked stroke")), *Error));
		const TArray<UStrokeData*> Strokes = {Stroke};  // Notify がない
		ASSERT_THAT(IsFalse(UTennisStrokeMetaBaker::BakeAppearance(Appearance, Strokes, Table, RacketMesh, Error)));
		ASSERT_THAT(IsTrue(Error.Contains(Stroke->GetName()), *Error));
		ASSERT_THAT(IsFalse(UTennisStrokeMetaBaker::BakeAppearance(nullptr, Strokes, Table, RacketMesh, Error)));
		ASSERT_THAT(IsFalse(UTennisStrokeMetaBaker::BakeAppearance(Appearance, {}, Table, RacketMesh, Error)));
		ASSERT_THAT(IsFalse(UTennisStrokeMetaBaker::BakeAppearance(Appearance, Strokes, nullptr, RacketMesh, Error)));
		ASSERT_THAT(IsFalse(UTennisStrokeMetaBaker::BakeAppearance(Appearance, Strokes, Table, nullptr, Error)));
		Appearance->Body.Reset();
		ASSERT_THAT(IsFalse(UTennisStrokeMetaBaker::BakeAppearance(Appearance, Strokes, Table, RacketMesh, Error)));
		ASSERT_THAT(AreEqual(0, Appearance->StrokeFits.Num()));
		ASSERT_THAT(IsFalse(Appearance->ReachBody.IsBaked()));
	}
};
#endif
