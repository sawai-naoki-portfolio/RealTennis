// W-3.4：Chooser の表を UStrokeData の条件から作り（条件の多い行が上。DD-03 §5.3）、表を評価して UStrokeData を選ぶ。
// 表を作る道具（TennisEditor の UTennisStrokeChooserBuilder）はエディタ専用なので、エディタのビルドだけでテストする。
#include "CQTest.h"
#include "Data/StrokeData.h"
#include "Player/TennisStrokeSelection.h"

#include <cmath>

#if WITH_EDITOR
#include "Chooser.h"
#include "ObjectChooser_Asset.h"
#include "TennisStrokeChooserBuilder.h"

TEST_CLASS(TennisStrokeChooser, "Tennis.Game.Stroke")
{
	UStrokeData* General = nullptr;
	UStrokeData* Specific = nullptr;
	UChooserTable* Table = nullptr;

	BEFORE_EACH()
	{
		// General：フォアならどれでも。Specific：フォアで、高さが Low か High（2 行に展開される）
		General = NewObject<UStrokeData>(GetTransientPackage(), TEXT("DA_Test_A_General"));
		General->SelectRows = { FStrokeSelectRow() };
		Specific = NewObject<UStrokeData>(GetTransientPackage(), TEXT("DA_Test_B_Specific"));
		FStrokeSelectRow Row;
		Row.Heights = { EStrokeHeightBand::Low, EStrokeHeightBand::High };
		Specific->SelectRows = { Row };
		Table = NewObject<UChooserTable>();
	}

	UStrokeData* Select(EStrokeSide Side, EStrokeHeightBand Height) const
	{
		FTennisStrokeChooserInput Input;
		Input.Side = Side;
		Input.Height = Height;
		return TennisStrokeSelection::Select(Table, Input);
	}

	// 行は条件の多い順（名前の順より優先）。複数の値は行に展開する。フォールバックは設定しない
	TEST_METHOD(Builder_PutsSpecificRowsFirstAndExpandsValues)
	{
		ASSERT_THAT(AreEqual(3, UTennisStrokeChooserBuilder::Rebuild(Table, { General, Specific, nullptr, General })));  // null と重複は飛ばす
		ASSERT_THAT(AreEqual(3, Table->ResultsStructs.Num()));
		ASSERT_THAT(IsTrue(Table->ResultsStructs[0].Get<FAssetChooser>().Asset == Specific));
		ASSERT_THAT(IsTrue(Table->ResultsStructs[1].Get<FAssetChooser>().Asset == Specific));
		ASSERT_THAT(IsTrue(Table->ResultsStructs[2].Get<FAssetChooser>().Asset == General));
		ASSERT_THAT(AreEqual(7, Table->ColumnsStructs.Num()));  // 列挙の 6 列と、打点の高さの範囲の列
		ASSERT_THAT(IsFalse(Table->FallbackResult.IsValid()));
		ASSERT_THAT(IsTrue(Table->OutputObjectType == UStrokeData::StaticClass()));
	}

	// 条件に合う行のうち、条件の多いものが選ばれる。どの行にも合わなければ nullptr（警告を出す）
	TEST_METHOD(Select_PicksTheMostSpecificMatchingRow)
	{
		UTennisStrokeChooserBuilder::Rebuild(Table, { General, Specific });
		ASSERT_THAT(IsTrue(Select(EStrokeSide::Forehand, EStrokeHeightBand::Low) == Specific));
		ASSERT_THAT(IsTrue(Select(EStrokeSide::Forehand, EStrokeHeightBand::High) == Specific));
		ASSERT_THAT(IsTrue(Select(EStrokeSide::Forehand, EStrokeHeightBand::Mid) == General));
		ASSERT_THAT(IsTrue(Select(EStrokeSide::Backhand, EStrokeHeightBand::Low) == nullptr));
	}

	// 同じ帯の中の高さの変種（DD-03 §5.3）：打点の高さが境界（float にした値）より下なら下の行、ちょうどと上なら上の行（下限を含み上限を含まない）
	TEST_METHOD(Select_PicksTheHeightVariantByHitHeight)
	{
		constexpr double Boundary = 0.93;
		UStrokeData* Upper = NewObject<UStrokeData>(GetTransientPackage(), TEXT("DA_Test_C_Upper"));
		FStrokeSelectRow UpperRow;
		UpperRow.bHasHitHeightMin = true;
		UpperRow.HitHeightMin = Boundary;
		Upper->SelectRows = { UpperRow };
		UStrokeData* Dip = NewObject<UStrokeData>(GetTransientPackage(), TEXT("DA_Test_B_Dip"));  // 名前の順では先（並びは下限で決まる）
		FStrokeSelectRow DipRow;
		DipRow.bHasHitHeightMax = true;
		DipRow.HitHeightMax = Boundary;
		Dip->SelectRows = { DipRow };
		ASSERT_THAT(AreEqual(2, UTennisStrokeChooserBuilder::Rebuild(Table, { Upper, Dip })));
		ASSERT_THAT(IsTrue(Table->ResultsStructs[0].Get<FAssetChooser>().Asset == Upper));

		const auto SelectAt = [this](double Height)
		{
			FTennisStrokeChooserInput Input;
			Input.HitHeight = Height;
			return TennisStrokeSelection::Select(Table, Input);
		};
		// 表の境界は float（Boundary を float にした値。丸めは 1e-7 m より小さい）。両方の行が境界の値を持ち、ちょうどの値は先の上の行
		const double Edge = static_cast<double>(static_cast<float>(Boundary));
		ASSERT_THAT(IsTrue(SelectAt(0.70) == Dip));
		ASSERT_THAT(IsTrue(SelectAt(Boundary - 1e-6) == Dip));
		ASSERT_THAT(IsTrue(SelectAt(std::nextafter(Edge, 0.0)) == Dip));
		ASSERT_THAT(IsTrue(SelectAt(Edge) == Upper));
		ASSERT_THAT(IsTrue(SelectAt(std::nextafter(Edge, 2.0)) == Upper));  // float の境界の間の double も、どちらかに入る（すき間がない）
		ASSERT_THAT(IsTrue(SelectAt(1.29) == Upper));
	}

	// 作り直すと、前の行は残らない
	TEST_METHOD(Builder_RebuildReplacesTheRows)
	{
		UTennisStrokeChooserBuilder::Rebuild(Table, { General, Specific });
		ASSERT_THAT(AreEqual(1, UTennisStrokeChooserBuilder::Rebuild(Table, { General })));
		ASSERT_THAT(IsTrue(Select(EStrokeSide::Forehand, EStrokeHeightBand::Low) == General));
		ASSERT_THAT(AreEqual(-1, UTennisStrokeChooserBuilder::Rebuild(nullptr, { General })));
	}
};
#endif
