// W-3.4：置いたストロークの表 CHT_StrokeSelect（Project Settings の StrokeChooser）と DA_Stroke_* のテスト。
// T-STRK-002：起こりうる入力（DD-03 §5.1 の決め方に通す候補と球種）のすべてに、モンタージュと正の ReleaseLeadTime・ChargeTime のある UStrokeData が選ばれる。
// T-STRK-010：選ばれる DA_Stroke_* の焼いた時刻が、今のモンタージュから焼き直した値と合う（手で入れた値・古い値がない。FR-STRK-007）。
// T-STRK-010・011（見た目のデータごと。DD-03 §7）：置いたすべての見た目のデータの StrokeFits・ReachBody が、その体で焼き直した値と
// 合い、ReachBody（打点の探索が体を置く位置）がその体のスイートスポットと合う。
// T-STRK-002：置いたすべての DA_Stroke_* がどれかの入力で選ばれ、置いた表は、置いた DA_Stroke_* から作り直した表と、どの入力でも同じ
// 行を選ぶ（データの範囲の抜け・作り直し忘れ・束縛の外れた列がない。DD-03 §5.3）。
#include "CQTest.h"
#include "Chooser.h"
#include "Core/TennisPlayerSettings.h"
#include "Data/StrokeData.h"
#include "Player/TennisPlayerAppearance.h"
#include "Player/TennisStrokeSelection.h"
#include "Stroke/TennisStrokeSelect.h"
#include "TennisDataAssetTestHelpers.h"

#include <cmath>

#if WITH_EDITOR
#include "Engine/StaticMesh.h"
#include "ObjectChooser_Asset.h"
#include "TennisStrokeChooserBuilder.h"
#include "TennisStrokeMetaBaker.h"
#endif

TEST_CLASS(TennisStrokeTable, "Tennis.Game.Stroke")
{
	const UChooserTable* Table = nullptr;

	BEFORE_EACH()
	{
		Table = GetDefault<UTennisPlayerSettings>()->StrokeChooser.LoadSynchronous();
		ASSERT_THAT(IsTrue(Table != nullptr, TEXT("Project Settings > Tennis Player > StrokeChooser")));
		ASSERT_THAT(IsFalse(Table->FallbackResult.IsValid()));  // フォールバックがあると、未定義の組み合わせが隠れる（DD-03 §5.3、RES-06 G-10）
	}

	/** 置いた DA_Stroke_* の行にある、打点の高さの範囲の境界（m。DD-03 §5.3 の高さの変種）。重なりは 1 つにする */
	static TArray<double> HitHeightBoundaries()
	{
		TArray<double> Out;
		for (const UStrokeData* Stroke : TennisTest::LoadAllDataAssets<UStrokeData>(TennisTest::StrokesFolder))
		{
			for (const FStrokeSelectRow& Row : Stroke->SelectRows)
			{
				if (Row.bHasHitHeightMin) { Out.AddUnique(Row.HitHeightMin); }
				if (Row.bHasHitHeightMax) { Out.AddUnique(Row.HitHeightMax); }
			}
		}
		return Out;
	}

	/** DD-03 §5.1 の決め方に通す入力：候補の種類 × 球種（ラリー）× 打点の高さ（帯の境界と高さの変種の境界のそれぞれの前後）× ストレッチ ×
	 *  向き × 打ち方 × 準備時間の長短 */
	template <typename FVisit>
	static void ForEachReachableInput(FVisit&& Visit)
	{
		const EHitKind Kinds[] = { EHitKind::Groundstroke, EHitKind::Volley, EHitKind::HalfVolley, EHitKind::Smash };
		const ETennisShotType Types[] = { ETennisShotType::Flat, ETennisShotType::Topspin, ETennisShotType::HeavyTopspin, ETennisShotType::Slice,
			ETennisShotType::Lob, ETennisShotType::SliceLob, ETennisShotType::Drop };
		using namespace TennisStrokeSelect;
		constexpr double Eps = 0.01;
		TArray<double> Heights = { 0.05, VeryLowBelow - Eps, VeryLowBelow, LowBelow - Eps, LowBelow, MidBelow - Eps, MidBelow,  // m（帯の境界）
			HighUpTo, HighUpTo + Eps, 3.20 };
		for (const double Boundary : HitHeightBoundaries())
		{
			const double Edge = static_cast<double>(static_cast<float>(Boundary));  // 表の境界は float（TennisStrokeChooserTests）
			Heights.Append({ Boundary - Eps, std::nextafter(Edge, 0.0), Edge, Boundary + Eps });
		}
		const double TimeBudgets[] = { DefaultBlockTimeBudget - 0.2, DefaultBlockTimeBudget + 0.7 };  // s（Block の境界の前後）
		for (const EHitKind Kind : Kinds)
		for (const ETennisShotType Type : Types)
		for (const double Height : Heights)
		for (const bool bStretch : { false, true })
		for (const EStrokeSide Side : { EStrokeSide::Forehand, EStrokeSide::Backhand })
		for (const EBackhandStyle Style : { EBackhandStyle::OneHanded, EBackhandStyle::TwoHanded })
		for (const double TimeBudget : TimeBudgets)
		{
			FHitCandidate Candidate;
			Candidate.Kind = Kind;
			Candidate.Side = Side;
			Candidate.bStretch = bStretch;
			Candidate.BallPos = FVector(-10.0, 1.0, Height);
			Candidate.Time = 10.0 + TimeBudget;
			Visit(TennisStrokeSelection::MakeInput(Candidate, Type, Style, 10.0));
		}
	}

	// T-STRK-002：どの入力にも、モンタージュと離す時刻の先行時間（自動の離しに使う。DD-03 §6.6）と溜めの時間（打球の溜め：§6.4）のある
	// UStrokeData が選ばれる（未定義 0）
	TEST_METHOD(EveryReachableInput_SelectsACompleteStroke)
	{
		int32 Inputs = 0;
		int32 Undefined = 0;
		FString FirstUndefined;
		TSet<const UStrokeData*> Selected;
		ForEachReachableInput([&](FTennisStrokeChooserInput Input)
		{
			++Inputs;
			const UStrokeData* Stroke = TennisStrokeSelection::Select(Table, Input);
			Selected.Add(Stroke);
			const TCHAR* Problem = Stroke == nullptr ? TEXT("no stroke")
				: Stroke->Montage == nullptr ? TEXT("no montage")
				: !(Stroke->ReleaseLeadTime > 0.0) ? TEXT("no positive ReleaseLeadTime")
				: !(Stroke->ChargeTime > 0.0) ? TEXT("no positive ChargeTime")
				: nullptr;
			if (Problem != nullptr && Undefined++ == 0)
			{
				FirstUndefined = FString::Printf(TEXT("%s (%s, ReleaseLeadTime %.3f, ChargeTime %.3f) for side %d, style %d, height %d (%.4f m), reach %d, family %d"),
					Problem, *GetNameSafe(Stroke), Stroke != nullptr ? Stroke->ReleaseLeadTime : 0.0, Stroke != nullptr ? Stroke->ChargeTime : 0.0,
					static_cast<int32>(Input.Side),
					static_cast<int32>(Input.BackhandStyle), static_cast<int32>(Input.Height), Input.HitHeight, static_cast<int32>(Input.Reach),
					static_cast<int32>(Input.Family));
			}
		});
		ASSERT_THAT(AreEqual(4 * 7 * (10 + 4 * HitHeightBoundaries().Num()) * 2 * 2 * 2 * 2, Inputs));
		ASSERT_THAT(IsTrue(Undefined == 0, *FString::Printf(TEXT("%d of %d inputs have no valid stroke; first: %s"), Undefined, Inputs, *FirstUndefined)));

		// 置いたすべての DA_Stroke_* が、どれかの入力で選ばれる（範囲が抜けて、上の行に隠れた変種がない）
		for (const UStrokeData* Stroke : TennisTest::LoadAllDataAssets<UStrokeData>(TennisTest::StrokesFolder))
		{
			ASSERT_THAT(IsTrue(Selected.Contains(Stroke), *FString::Printf(TEXT("%s is never selected"), *Stroke->GetName())));
		}
	}

	/** 中くらいの高さのドライブ（DD-03 §12.1 の M3 の対象）で表が選ぶストローク */
	const UStrokeData* SelectMidDrive(EStrokeSide Side, EBackhandStyle Style) const
	{
		FTennisStrokeChooserInput Input = TennisStrokeSelection::MidDriveInput(Side, Style);
		return TennisStrokeSelection::Select(Table, Input);
	}

	// T-STRK-011（見た目のデータごと）：焼いた探索の体の位置が、中くらいの高さのドライブで選ばれるストロークの、その体の焼いた
	// スイートスポットと、横・前とも 5 cm 以内で合う（右利き：フォアは +Y）
	TEST_METHOD(AppearanceReachBodies_MatchTheirSweetSpots)
	{
		const TArray<const UTennisPlayerAppearance*> Appearances = TennisTest::LoadAllDataAssets<UTennisPlayerAppearance>(TennisTest::PlayersFolder);
		ASSERT_THAT(IsTrue(Appearances.Num() > 0));
		for (const UTennisPlayerAppearance* Appearance : Appearances)
		{
			const FTennisReachBody& Reach = Appearance->ReachBody;
			ASSERT_THAT(IsTrue(Reach.IsBaked(), *FString::Printf(TEXT("%s: run bake_player_appearances"), *Appearance->GetName())));
			for (const EStrokeSide Side : { EStrokeSide::Forehand, EStrokeSide::Backhand })
			for (const EBackhandStyle Style : { EBackhandStyle::OneHanded, EBackhandStyle::TwoHanded })
			{
				const UStrokeData* MidDrive = SelectMidDrive(Side, Style);
				// 焼きの入力（打点の高さ MidDriveHitHeight）は、高さの変種の上の行を選ぶ（DD-03 §4.2。境界が 1.0 m を超えると Dip から焼いてしまう）
				ASSERT_THAT(IsTrue(MidDrive != nullptr && !MidDrive->SelectRows.ContainsByPredicate([](const FStrokeSelectRow& Row) { return Row.bHasHitHeightMax; }),
					*GetNameSafe(MidDrive)));
				const FTennisStrokeFit* Fit = Appearance->FindStrokeFit(MidDrive);
				ASSERT_THAT(IsTrue(Fit != nullptr, *Appearance->GetName()));
				const FVector Baked = Fit->SweetSpotOffset.GetLocation();
				const double Lateral = Side == EStrokeSide::Forehand ? Reach.ForehandSide
					: -(Style == EBackhandStyle::OneHanded ? Reach.OneHandedBackhandSide : Reach.TwoHandedBackhandSide);
				ASSERT_THAT(IsTrue(FMath::Abs(Baked.Y - Lateral) <= 0.05 && FMath::Abs(Baked.X - Reach.Forward) <= 0.05,
					*FString::Printf(TEXT("%s: baked (%.3f, %.3f), reach (%.3f, %.3f)"), *Appearance->GetName(), Baked.X, Baked.Y, Reach.Forward, Lateral)));
			}
		}
	}

#if WITH_EDITOR
	// T-STRK-002：置いた表は、置いた DA_Stroke_* から作り直した表と、行の順・列の数が同じで、どの入力でも同じストロークを選ぶ（DD-03 §5.3。
	// 束縛の外れた範囲の列は、すべての行を通すので、高さの変種の境界の前後で選ぶものが変わる）
	TEST_METHOD(PlacedTable_SelectsLikeARebuild)
	{
		TArray<UStrokeData*> Strokes;
		for (const UStrokeData* Stroke : TennisTest::LoadAllDataAssets<UStrokeData>(TennisTest::StrokesFolder))
		{
			Strokes.Add(const_cast<UStrokeData*>(Stroke));  // 表を作る道具は読むだけ
		}
		UChooserTable* Rebuilt = NewObject<UChooserTable>(GetTransientPackage());
		ASSERT_THAT(AreEqual(Table->ResultsStructs.Num(), UTennisStrokeChooserBuilder::Rebuild(Rebuilt, Strokes)));
		ASSERT_THAT(AreEqual(Table->ColumnsStructs.Num(), Rebuilt->ColumnsStructs.Num()));
		for (int32 Row = 0; Row < Rebuilt->ResultsStructs.Num(); ++Row)
		{
			const FAssetChooser* Placed = Table->ResultsStructs[Row].GetPtr<FAssetChooser>();  // 型が違えば null（Get は check で止まる）
			const FAssetChooser* Fresh = Rebuilt->ResultsStructs[Row].GetPtr<FAssetChooser>();
			ASSERT_THAT(IsTrue(Placed != nullptr && Fresh != nullptr && Placed->Asset == Fresh->Asset, *FString::Printf(TEXT("row %d"), Row)));
		}

		int32 Different = 0;
		FString FirstDifferent;
		ForEachReachableInput([&](FTennisStrokeChooserInput Input)
		{
			const UStrokeData* Placed = TennisStrokeSelection::Select(Table, Input);
			const UStrokeData* Fresh = TennisStrokeSelection::Select(Rebuilt, Input);
			if (Placed != Fresh && Different++ == 0)
			{
				FirstDifferent = FString::Printf(TEXT("placed %s, rebuilt %s at side %d, style %d, height %d (%.4f m), reach %d, family %d"),
					*GetNameSafe(Placed), *GetNameSafe(Fresh), static_cast<int32>(Input.Side), static_cast<int32>(Input.BackhandStyle),
					static_cast<int32>(Input.Height), Input.HitHeight, static_cast<int32>(Input.Reach), static_cast<int32>(Input.Family));
			}
		});
		ASSERT_THAT(IsTrue(Different == 0, *FString::Printf(TEXT("%d inputs differ; first: %s"), Different, *FirstDifferent)));
	}

	// T-STRK-010：選ばれる DA_Stroke_* の焼いた時刻は、今のモンタージュから焼き直した値と合う（複製を焼くので、アセットは変えない）
	TEST_METHOD(SelectedStrokes_MatchARebake)
	{
		TArray<const UStrokeData*> Strokes;  // 選ばれた順（失敗の報告を毎回同じにする）
		ForEachReachableInput([&](FTennisStrokeChooserInput Input) { Strokes.AddUnique(TennisStrokeSelection::Select(Table, Input)); });
		Strokes.Remove(nullptr);
		ASSERT_THAT(IsTrue(Strokes.Num() > 0));

		for (const UStrokeData* Stroke : Strokes)
		{
			UStrokeData* Copy = DuplicateObject<UStrokeData>(Stroke, GetTransientPackage());
			FString Error;
			ASSERT_THAT(IsTrue(UTennisStrokeMetaBaker::Bake(Copy, Error), *FString::Printf(TEXT("%s: %s"), *Stroke->GetName(), *Error)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Copy->ImpactTime, Stroke->ImpactTime, 1e-4)
				&& FMath::IsNearlyEqual(Copy->FollowThroughLock, Stroke->FollowThroughLock, 1e-4), *Stroke->GetName()));
			ASSERT_THAT(IsTrue(Stroke->ImpactTime > 0.0));
		}
	}

	// T-STRK-010・011（見た目のデータごと）：置いたすべての見た目のデータの焼いた値（すべての DA_Stroke_* のスイートスポットと探索の
	// 体の位置）が、その体で焼き直した値と合う。焼いていない組・古い値がない（複製を焼くので、アセットは変えない。DD-03 §7）
	TEST_METHOD(Appearances_MatchARebake)
	{
		TArray<UStrokeData*> Strokes;
		for (const UStrokeData* Stroke : TennisTest::LoadAllDataAssets<UStrokeData>(TennisTest::StrokesFolder))
		{
			Strokes.Add(const_cast<UStrokeData*>(Stroke));  // 焼く道具は読むだけ
		}
		const TArray<const UTennisPlayerAppearance*> Appearances = TennisTest::LoadAllDataAssets<UTennisPlayerAppearance>(TennisTest::PlayersFolder);
		UStaticMesh* Racket = GetDefault<UTennisPlayerSettings>()->RacketMesh.LoadSynchronous();
		ASSERT_THAT(IsTrue(Strokes.Num() > 0 && Appearances.Num() > 0 && Racket != nullptr));
		for (const UTennisPlayerAppearance* Appearance : Appearances)
		{
			UTennisPlayerAppearance* Copy = DuplicateObject<UTennisPlayerAppearance>(Appearance, GetTransientPackage());
			FString Error;
			ASSERT_THAT(IsTrue(UTennisStrokeMetaBaker::BakeAppearance(Copy, Strokes, Table, Racket, Error),
				*FString::Printf(TEXT("%s: %s"), *Appearance->GetName(), *Error)));
			ASSERT_THAT(AreEqual(Copy->StrokeFits.Num(), Appearance->StrokeFits.Num(), *Appearance->GetName()));
			for (int32 Index = 0; Index < Copy->StrokeFits.Num(); ++Index)
			{
				const FTennisStrokeFit& Rebaked = Copy->StrokeFits[Index];
				const FTennisStrokeFit& Stored = Appearance->StrokeFits[Index];
				ASSERT_THAT(IsTrue(Rebaked.Stroke == Stored.Stroke
					&& Rebaked.SweetSpotOffset.GetLocation().Equals(Stored.SweetSpotOffset.GetLocation(), 0.001)  // 1 mm
					&& Rebaked.SweetSpotOffset.GetRotation().Equals(Stored.SweetSpotOffset.GetRotation(), 1e-4),
					*FString::Printf(TEXT("%s: %s"), *Appearance->GetName(), *Rebaked.Stroke.ToString())));
			}
			const FTennisReachBody& A = Copy->ReachBody;
			const FTennisReachBody& B = Appearance->ReachBody;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(A.Forward, B.Forward, 0.001) && FMath::IsNearlyEqual(A.ForehandSide, B.ForehandSide, 0.001)
				&& FMath::IsNearlyEqual(A.OneHandedBackhandSide, B.OneHandedBackhandSide, 0.001)
				&& FMath::IsNearlyEqual(A.TwoHandedBackhandSide, B.TwoHandedBackhandSide, 0.001), *Appearance->GetName()));
		}
	}
#endif
};
