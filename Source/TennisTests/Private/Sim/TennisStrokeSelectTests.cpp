// W-3.4：ストロークの選択の入力の決め方（DD-03 §5.1、T-STRK-009）。高さの帯・振り方の系統・届き方。
#include "CQTest.h"
#include "Stroke/TennisStrokeSelect.h"

TEST_CLASS(TennisStrokeSelectInput, "Tennis.Sim.Stroke")
{
	// 帯の境界（0.4・0.7・1.3・2.2 m）：下の境界は上の帯に入り、High は 2.2 m まで（ボレーの上端）。すき間がない
	TEST_METHOD(HeightBand_BoundariesHaveNoGap)
	{
		struct FCase { double Z; EStrokeHeightBand Band; };
		const FCase Cases[] = {
			{ 0.0, EStrokeHeightBand::VeryLow }, { 0.399, EStrokeHeightBand::VeryLow }, { 0.4, EStrokeHeightBand::Low },
			{ 0.699, EStrokeHeightBand::Low }, { 0.7, EStrokeHeightBand::Mid }, { 1.0, EStrokeHeightBand::Mid },
			{ 1.299, EStrokeHeightBand::Mid }, { 1.3, EStrokeHeightBand::High }, { 1.9, EStrokeHeightBand::High },
			{ 2.0, EStrokeHeightBand::High }, { 2.2, EStrokeHeightBand::High }, { 2.201, EStrokeHeightBand::Overhead },
			{ 3.2, EStrokeHeightBand::Overhead }, { -0.1, EStrokeHeightBand::VeryLow },
		};
		for (const FCase& Case : Cases)
		{
			ASSERT_THAT(IsTrue(TennisStrokeSelect::HeightBandOf(Case.Z) == Case.Band, *FString::Printf(TEXT("z=%.3f"), Case.Z)));
		}
	}

	// 数でない高さは Mid（警告を出す）
	TEST_METHOD(HeightBand_InvalidInputFallsBackToMid)
	{
		ASSERT_THAT(IsTrue(TennisStrokeSelect::HeightBandOf(std::numeric_limits<double>::quiet_NaN()) == EStrokeHeightBand::Mid));
		ASSERT_THAT(IsTrue(TennisStrokeSelect::HeightBandOf(std::numeric_limits<double>::infinity()) == EStrokeHeightBand::Mid));
	}

	// ボレー・ハーフボレー・スマッシュは、球種や準備時間によらず、その種類
	TEST_METHOD(Family_NetAndOverheadKindsWin)
	{
		for (const ETennisShotType Type : { ETennisShotType::Flat, ETennisShotType::Slice, ETennisShotType::Lob, ETennisShotType::Drop })
		{
			for (const double Budget : { 0.05, 1.0 })
			{
				ASSERT_THAT(IsTrue(TennisStrokeSelect::FamilyOf(EHitKind::Volley, Type, Budget, 0.3) == EStrokeFamily::Volley));
				ASSERT_THAT(IsTrue(TennisStrokeSelect::FamilyOf(EHitKind::HalfVolley, Type, Budget, 0.3) == EStrokeFamily::HalfVolley));
				ASSERT_THAT(IsTrue(TennisStrokeSelect::FamilyOf(EHitKind::Smash, Type, Budget, 0.3) == EStrokeFamily::Smash));
			}
		}
	}

	// グラウンドストロークは球種で決まる
	TEST_METHOD(Family_GroundstrokeFollowsTheShotType)
	{
		struct FCase { ETennisShotType Type; EStrokeFamily Family; };
		const FCase Cases[] = {
			{ ETennisShotType::Flat, EStrokeFamily::Drive }, { ETennisShotType::Topspin, EStrokeFamily::Drive },
			{ ETennisShotType::HeavyTopspin, EStrokeFamily::Drive }, { ETennisShotType::Slice, EStrokeFamily::Slice },
			{ ETennisShotType::Lob, EStrokeFamily::Lob }, { ETennisShotType::SliceLob, EStrokeFamily::Lob },
			{ ETennisShotType::Drop, EStrokeFamily::Drop },
			{ ETennisShotType::ServeFlat, EStrokeFamily::Drive },  // 合わない球種は Drive（警告を出す）
		};
		for (const FCase& Case : Cases)
		{
			ASSERT_THAT(IsTrue(TennisStrokeSelect::FamilyOf(EHitKind::Groundstroke, Case.Type, 1.0, 0.3) == Case.Family,
				*FString::Printf(TEXT("type=%d"), static_cast<int32>(Case.Type))));
		}
	}

	// グラウンドストロークで準備時間が BlockTimeBudget より短ければ Block（ちょうどは Block にしない）
	TEST_METHOD(Family_ShortTimeBudgetBlocks)
	{
		ASSERT_THAT(IsTrue(TennisStrokeSelect::FamilyOf(EHitKind::Groundstroke, ETennisShotType::Topspin, 0.299, 0.3) == EStrokeFamily::Block));
		ASSERT_THAT(IsTrue(TennisStrokeSelect::FamilyOf(EHitKind::Groundstroke, ETennisShotType::Lob, 0.0, 0.3) == EStrokeFamily::Block));
		ASSERT_THAT(IsTrue(TennisStrokeSelect::FamilyOf(EHitKind::Groundstroke, ETennisShotType::Topspin, 0.3, 0.3) == EStrokeFamily::Drive));
	}

	TEST_METHOD(Reach_StretchOrNormal)
	{
		ASSERT_THAT(IsTrue(TennisStrokeSelect::ReachOf(true) == EStrokeReach::Stretch));
		ASSERT_THAT(IsTrue(TennisStrokeSelect::ReachOf(false) == EStrokeReach::Normal));
	}
};
