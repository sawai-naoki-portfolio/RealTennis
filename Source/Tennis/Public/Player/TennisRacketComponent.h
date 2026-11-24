// ラケット（DD-03 §7）：選手のメッシュの手のソケット（racket_attach_r・_l）に、ラケットのメッシュのソケット grip を重ねて付ける。
// スイートスポット（ソケット racket_sweetspot）のワールドの位置を返す（インパクトの判定：W-3.5）。見た目だけで、衝突はしない。
#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "TennisRacketComponent.generated.h"

UCLASS(ClassGroup = (Tennis))
class TENNIS_API UTennisRacketComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	/** ラケットのメッシュのソケット（ART-01 §2 A-RKT-001） */
	static const FName GripSocket;
	static const FName SweetSpotSocket;

	UTennisRacketComponent();

	/** メッシュを決め、ソケット grip が親（手）のソケットに重なるように相対の位置と向きを決める。メッシュか grip がなければ false */
	bool SetRacketMesh(UStaticMesh* Mesh);

	/** スイートスポットのワールドの位置（cm） */
	FVector GetSweetSpotWorld() const;
};
