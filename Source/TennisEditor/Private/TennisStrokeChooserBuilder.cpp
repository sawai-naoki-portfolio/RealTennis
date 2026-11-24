#include "TennisStrokeChooserBuilder.h"

#include "Chooser.h"
#include "ChooserPropertyAccess.h"
#include "Data/StrokeData.h"
#include "EnumColumn.h"
#include "FloatRangeColumn.h"
#include "ObjectChooser_Asset.h"
#include "Player/TennisStrokeSelection.h"
#include "TennisSimLog.h"

namespace
{
	constexpr int32 ColumnCount = 6;

	/** 表の 1 行：ストロークと、列ごとの値（未設定は Any）と、打点の高さの範囲（未設定は範囲なし） */
	struct FBuiltRow
	{
		UStrokeData* Stroke = nullptr;
		TOptional<uint8> Values[ColumnCount];
		TOptional<double> HitHeightMin;
		TOptional<double> HitHeightMax;
		int32 Conditions = 0;
	};

	/** 列：FTennisStrokeChooserInput のプロパティの名前と、その列挙 */
	struct FColumnDef
	{
		FName Property;
		const UEnum* Enum;
	};

	TArray<FColumnDef> ColumnDefs()
	{
		return {
			{ GET_MEMBER_NAME_CHECKED(FTennisStrokeChooserInput, Side), StaticEnum<EStrokeSide>() },
			{ GET_MEMBER_NAME_CHECKED(FTennisStrokeChooserInput, BackhandStyle), StaticEnum<EBackhandStyle>() },
			{ GET_MEMBER_NAME_CHECKED(FTennisStrokeChooserInput, Height), StaticEnum<EStrokeHeightBand>() },
			{ GET_MEMBER_NAME_CHECKED(FTennisStrokeChooserInput, Reach), StaticEnum<EStrokeReach>() },
			{ GET_MEMBER_NAME_CHECKED(FTennisStrokeChooserInput, Family), StaticEnum<EStrokeFamily>() },
			{ GET_MEMBER_NAME_CHECKED(FTennisStrokeChooserInput, Stance), StaticEnum<EStrokeStance>() },
		};
	}

	template <typename EnumType>
	TArray<TOptional<uint8>> Choices(const TArray<EnumType>& Values)
	{
		TArray<TOptional<uint8>> Out;
		for (const EnumType Value : Values)
		{
			Out.AddUnique(TOptional<uint8>(static_cast<uint8>(Value)));
		}
		if (Out.IsEmpty())
		{
			Out.Add(TOptional<uint8>());  // Any
		}
		return Out;
	}

	/** SelectRows の 1 行を、値の組み合わせの数だけの行に展開する */
	void Expand(UStrokeData* Stroke, const FStrokeSelectRow& Select, TArray<FBuiltRow>& Out)
	{
		const TArray<TOptional<uint8>> PerColumn[ColumnCount] = {
			{ TOptional<uint8>(static_cast<uint8>(Select.Side)) },
			Choices(Select.BackhandStyles), Choices(Select.Heights), Choices(Select.Reaches), Choices(Select.Families), Choices(Select.Stances),
		};
		FBuiltRow First{ Stroke };
		if (Select.bHasHitHeightMin)
		{
			First.HitHeightMin = Select.HitHeightMin;
		}
		if (Select.bHasHitHeightMax)
		{
			First.HitHeightMax = Select.HitHeightMax;
		}
		First.Conditions = Select.bHasHitHeightMin || Select.bHasHitHeightMax ? 1 : 0;  // 範囲の列は 1 つの条件（DD-03 §5.3 の行の順）
		const bool bBadMin = Select.bHasHitHeightMin && !FMath::IsFinite(Select.HitHeightMin);
		const bool bBadMax = Select.bHasHitHeightMax && !FMath::IsFinite(Select.HitHeightMax);
		if (bBadMin || bBadMax || (Select.bHasHitHeightMin && Select.bHasHitHeightMax))
		{
			// 両端のある範囲は、上限を「含まない」にできない（隣の行より先に並ぶ）。数でない・逆の範囲の行は、どの入力でも選ばれない
			UE_LOG(LogTennisStroke, Warning, TEXT("[ChooserBuilder] %s: a hit height range with both ends or a non-finite end min=%s max=%s (use one end only: DD-03 §5.3)"),
				*Stroke->GetName(), Select.bHasHitHeightMin ? *FString::SanitizeFloat(Select.HitHeightMin) : TEXT("none"),
				Select.bHasHitHeightMax ? *FString::SanitizeFloat(Select.HitHeightMax) : TEXT("none"));
		}
		TArray<FBuiltRow> Rows = { First };
		for (int32 Column = 0; Column < ColumnCount; ++Column)
		{
			TArray<FBuiltRow> Next;
			for (const FBuiltRow& Row : Rows)
			{
				for (const TOptional<uint8>& Value : PerColumn[Column])
				{
					FBuiltRow& Added = Next.Add_GetRef(Row);
					Added.Values[Column] = Value;
					Added.Conditions += Value.IsSet() ? 1 : 0;
				}
			}
			Rows = MoveTemp(Next);
		}
		Out.Append(Rows);
	}

	FInstancedStruct MakeColumn(const FColumnDef& Def, const TArray<FBuiltRow>& Rows, int32 Column)
	{
		FInstancedStruct ColumnStruct;
		ColumnStruct.InitializeAs<FEnumColumn>();
		FEnumColumn& EnumColumn = ColumnStruct.GetMutable<FEnumColumn>();
		EnumColumn.InputValue.InitializeAs<FEnumContextProperty>();
		FChooserEnumPropertyBinding& Binding = EnumColumn.InputValue.GetMutable<FEnumContextProperty>().Binding;
		Binding.PropertyBindingChain = { Def.Property };
		Binding.ContextIndex = 0;
		Binding.Enum = Def.Enum;
		Binding.DisplayName = Def.Property.ToString();
		for (const FBuiltRow& Row : Rows)
		{
			FChooserEnumRowData& Cell = EnumColumn.RowValues.AddDefaulted_GetRef();
			const TOptional<uint8>& Value = Row.Values[Column];
			Cell.Comparison = Value.IsSet() ? EEnumColumnCellValueComparison::MatchEqual : EEnumColumnCellValueComparison::MatchAny;
			Cell.Value = Value.Get(0);
			Cell.ValueName = Value.IsSet() ? Def.Enum->GetNameByValue(Value.GetValue()) : NAME_None;
		}
		return ColumnStruct;
	}
}

namespace
{
	/**
	 * 打点の高さの範囲の列（FFloatRangeColumn。DD-03 §5.3）。Chooser の範囲は float で両端を含み、入力は double なので、隣り合う行は境界の値を
	 * 共有する（上限を 1 つ小さい float にすると、その間の入力がどちらにも入らない）。境界のちょうどの値は、行の順（下限のある行が先）で上の行になる
	 */
	FInstancedStruct MakeHitHeightColumn(const TArray<FBuiltRow>& Rows)
	{
		FInstancedStruct ColumnStruct;
		ColumnStruct.InitializeAs<FFloatRangeColumn>();
		FFloatRangeColumn& RangeColumn = ColumnStruct.GetMutable<FFloatRangeColumn>();
		RangeColumn.InputValue.InitializeAs<FFloatContextProperty>();
		FChooserPropertyBinding& Binding = RangeColumn.InputValue.GetMutable<FFloatContextProperty>().Binding;
		Binding.PropertyBindingChain = { GET_MEMBER_NAME_CHECKED(FTennisStrokeChooserInput, HitHeight) };
		Binding.ContextIndex = 0;
		Binding.DisplayName = TEXT("HitHeight");
		for (const FBuiltRow& Row : Rows)
		{
			FChooserFloatRangeRowData& Cell = RangeColumn.RowValues.AddDefaulted_GetRef();
			Cell.bNoMin = !Row.HitHeightMin.IsSet();
			Cell.Min = static_cast<float>(Row.HitHeightMin.Get(0.0));
			Cell.bNoMax = !Row.HitHeightMax.IsSet();
			Cell.Max = static_cast<float>(Row.HitHeightMax.Get(0.0));
		}
		return ColumnStruct;
	}
}

int32 UTennisStrokeChooserBuilder::Rebuild(UChooserTable* Table, const TArray<UStrokeData*>& Strokes)
{
	if (Table == nullptr)
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[ChooserBuilder] no chooser table"));
		return -1;
	}

	TArray<UStrokeData*> Sorted;
	for (UStrokeData* Stroke : Strokes)
	{
		if (Stroke == nullptr)
		{
			UE_LOG(LogTennisStroke, Warning, TEXT("[ChooserBuilder] %s: skipped a null stroke"), *Table->GetName());
			continue;
		}
		Sorted.AddUnique(Stroke);
	}
	Sorted.Sort([](const UStrokeData& A, const UStrokeData& B) { return A.GetName() < B.GetName(); });

	TArray<FBuiltRow> Rows;
	for (UStrokeData* Stroke : Sorted)
	{
		for (const FStrokeSelectRow& Select : Stroke->SelectRows)
		{
			Expand(Stroke, Select, Rows);
		}
	}
	// 条件の多い行が上。同じ数なら、打点の高さの下限のある行が先（境界のちょうどの値は上の行：DD-03 §5.3）、その次は名前の順（決定論）
	Rows.StableSort([](const FBuiltRow& A, const FBuiltRow& B)
	{
		if (A.Conditions != B.Conditions)
		{
			return A.Conditions > B.Conditions;
		}
		return A.HitHeightMin.IsSet() && !B.HitHeightMin.IsSet();
	});

	Table->Modify();
	Table->ResultType = EObjectChooserResultType::ObjectResult;
	Table->OutputObjectType = UStrokeData::StaticClass();
	Table->ContextData.Reset();
	Table->ContextData.AddDefaulted_GetRef().InitializeAs<FContextObjectTypeStruct>();
	Table->ContextData[0].GetMutable<FContextObjectTypeStruct>().Struct = FTennisStrokeChooserInput::StaticStruct();
	Table->FallbackResult.Reset();  // 未定義の組み合わせを隠さない（DD-03 §5.3）

	Table->ResultsStructs.Reset();
	Table->DisabledRows.Reset();
	for (const FBuiltRow& Row : Rows)
	{
		Table->ResultsStructs.AddDefaulted_GetRef().InitializeAs<FAssetChooser>();
		Table->ResultsStructs.Last().GetMutable<FAssetChooser>().Asset = Row.Stroke;
		Table->DisabledRows.Add(false);
	}
	Table->ColumnsStructs.Reset();
	const TArray<FColumnDef> Defs = ColumnDefs();
	for (int32 Column = 0; Column < Defs.Num(); ++Column)
	{
		Table->ColumnsStructs.Add(MakeColumn(Defs[Column], Rows, Column));
	}
	Table->ColumnsStructs.Add(MakeHitHeightColumn(Rows));
	Table->Compile(true);
	Table->MarkPackageDirty();
	UE_LOG(LogTennisStroke, Log, TEXT("[ChooserBuilder] %s: %d row(s) from %d stroke(s)"), *Table->GetName(), Rows.Num(), Sorted.Num());
	return Rows.Num();
}
