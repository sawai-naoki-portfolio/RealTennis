// 選手のカメラ（DD-07 §1、UML-02 §7）：操作する選手・コート・ボールから文脈を作り、カメラのモードに視点を決めさせる。
// W-3.8 では中継カメラ（UTennisCameraMode_Broadcast）だけを持つ。モードの切り替え（UTennisCameraDirector）は W-5.10・W-7.1。
#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "TennisPlayerCameraManager.generated.h"

class ATennisBall;
class UTennisCameraModeBase;
struct FTennisCameraContext;

UCLASS()
class TENNIS_API ATennisPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	ATennisPlayerCameraManager();

	UTennisCameraModeBase* GetMode() const { return Mode; }

protected:
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;

private:
	/** 選手・コートがなければ false（ボールがなければ選手を見る） */
	bool MakeContext(FTennisCameraContext& Out);

	UPROPERTY(VisibleAnywhere, Instanced, Category = "Camera")
	TObjectPtr<UTennisCameraModeBase> Mode;

	TWeakObjectPtr<ATennisBall> Ball;
	TWeakObjectPtr<const APawn> EvaluatedPawn;
	bool bWarnedNoContext = false;
};
