// Chooser の表 CHT_StrokeSelect を、UStrokeData の条件（SelectRows）から作り直す（DD-03 §5.3）。
// 表の列（FEnumColumn）は Python から作れないので C++ で作る。アセットを探す・作る・保存するのは MCP のツール
// （Tools/Unreal の TennisTools.rebuild_stroke_chooser）が行い、ここは渡された表の中身だけを書き換える。
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TennisStrokeChooserBuilder.generated.h"

class UChooserTable;
class UStrokeData;

UCLASS()
class TENNISEDITOR_API UTennisStrokeChooserBuilder : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Table の入力（FTennisStrokeChooserInput）・列（Side・BackhandStyle・Height・Reach・Family・Stance と、打点の高さ HitHeight の範囲）・
	 * 行を作り直す。行は Strokes の SelectRows を展開したもの（複数の値は、その数だけの行）。条件（Any でない列。範囲の列は、下限・
	 * 上限のどちらかがあれば 1 つ）の多い行を上にし、同じ数なら打点の高さの下限のある行が先、その次に UStrokeData の名前の順、
	 * 同じストロークの中では SelectRows の順にする（DD-03 §5.3）。フォールバックは設定しない。
	 * null のストロークは飛ばす（警告を出す）。作った行の数を返す。Table が null なら −1（警告を出す）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Tennis|Editor")
	static int32 Rebuild(UChooserTable* Table, const TArray<UStrokeData*>& Strokes);
};
