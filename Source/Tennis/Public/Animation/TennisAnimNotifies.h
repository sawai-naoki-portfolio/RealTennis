// ストロークのモンタージュに置く印（ART-02 §5）。時刻は StrokeMetaBaker が読み、UStrokeData に書く（DD-03 §5.2）。
// 実行時にインパクトをストロークへ知らせる処理（DD-03 §6.4）は、UTennisStrokeComponent と一緒に足す（W-3.5）。
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "TennisAnimNotifies.generated.h"

/** ラケットがボールに当たるフレーム（必須。1 つのモンタージュに 1 つ） */
UCLASS(meta = (DisplayName = "Tennis Impact"))
class TENNIS_API UAnimNotify_TennisImpact : public UAnimNotify
{
	GENERATED_BODY()
};

/** フォロースルーで、移動を始められるフレーム（インパクトより後。1 つのモンタージュに 1 つ） */
UCLASS(meta = (DisplayName = "Tennis Recover Start"))
class TENNIS_API UAnimNotify_TennisRecoverStart : public UAnimNotify
{
	GENERATED_BODY()
};
