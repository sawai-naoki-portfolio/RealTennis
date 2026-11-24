// 中継カメラ（既定。DD-07 §1.1、FR-CAM-001）：自分の側のベースラインの後ろの上から、選手とボールを強いダンピングで追う。
// 相手のベースラインが画面から切れないように、最後の向きと FOV に制約を掛ける。計算は TennisBroadcastCamera。
#pragma once

#include "CoreMinimal.h"
#include "Camera/TennisBroadcastCamera.h"
#include "Camera/TennisCameraModeBase.h"
#include "TennisCameraMode_Broadcast.generated.h"

UCLASS()
class TENNIS_API UTennisCameraMode_Broadcast : public UTennisCameraModeBase
{
	GENERATED_BODY()

public:
	/** 調整の値（DATA-02 §8） */
	UPROPERTY(EditAnywhere, Category = "Camera")
	FTennisBroadcastCameraParams Params;

	virtual FMinimalViewInfo Evaluate(const FTennisCameraContext& Context, float DeltaSeconds) override;
	virtual void Reset() override;

private:
	FTennisCameraSpring Location;
	FTennisCameraSpring LookAt;
	double LastSide = 0.0;  // 0：まだ評価していない（次の評価で目標にそろえる）
};
