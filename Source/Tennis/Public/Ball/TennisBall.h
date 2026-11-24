// ボールのアクタ（クラス図 §3）。見た目のメッシュと、シミュレーションのコンポーネントを持つ。
// メッシュのアセットはレベル・ブループリントで設定する（C++ にアセットのパスを書かない）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TennisBall.generated.h"

class UStaticMeshComponent;
class UTennisBallSimComponent;

UCLASS()
class TENNIS_API ATennisBall : public AActor
{
	GENERATED_BODY()

public:
	ATennisBall();

	UTennisBallSimComponent* GetSim() const { return Sim; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Tennis")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Tennis")
	TObjectPtr<UTennisBallSimComponent> Sim;
};
