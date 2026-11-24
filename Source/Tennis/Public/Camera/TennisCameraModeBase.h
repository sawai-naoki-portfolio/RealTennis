// カメラのモードの基底（DD-07 §1、UML-02 §7）。モードを増やすときは、この派生を足す（開発ルール集（非公開） §4 O）。
// どの派生も、同じ文脈から破綻しない視点を返す（L）。
#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"
#include "UObject/Object.h"
#include "TennisCameraModeBase.generated.h"

/** モードが視点を決めるための入力。位置はコート局所（m） */
struct FTennisCameraContext
{
	FTransform CourtTransform = FTransform::Identity;
	FVector Player = FVector::ZeroVector;
	FVector Ball = FVector::ZeroVector;  // ボールがなければ選手の位置
	double Side = -1.0;                  // 自分の側（−1：X < 0、+1：X > 0）
	double ViewAspect = 16.0 / 9.0;      // 今の画面の縦横比
};

UCLASS(Abstract, DefaultToInstanced, EditInlineNew)
class TENNIS_API UTennisCameraModeBase : public UObject
{
	GENERATED_BODY()

public:
	/** 視点（ワールド）を返す。DeltaSeconds は前の評価からの時間（演出の値） */
	virtual FMinimalViewInfo Evaluate(const FTennisCameraContext& Context, float DeltaSeconds)
		PURE_VIRTUAL(UTennisCameraModeBase::Evaluate, return FMinimalViewInfo(););

	/** 次の評価で目標にそろえる（選手が替わったとき・始まり） */
	virtual void Reset() {}
};
