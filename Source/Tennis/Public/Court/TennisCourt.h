// コートのアクタ（DD-01 §1、ARCH-01）。原点はネット中央の地面。シミュレーションのコート局所の座標（m）と
// UE のワールド（cm）の変換は、ここだけで行う（ARCH-03 U-2：ほかで × 100 や ÷ 100 を書かない）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TennisCourt.generated.h"

UCLASS()
class TENNIS_API ATennisCourt : public AActor
{
	GENERATED_BODY()

public:
	static constexpr double UnrealUnitsPerMeter = 100.0;

	ATennisCourt();

	/** コート局所の位置（m）→ ワールドの位置（cm）。 */
	FVector LocalMetersToWorld(const FVector& LocalMeters) const;

	/** ワールドの位置（cm）→ コート局所の位置（m）。 */
	FVector WorldToLocalMeters(const FVector& World) const;

	/** 上の 2 つの本体（テストのため Transform を渡せる形）。アクタの拡大・縮小は無視する（コートは常に実寸）。 */
	static FVector LocalMetersToWorld(const FTransform& CourtTransform, const FVector& LocalMeters);
	static FVector WorldToLocalMeters(const FTransform& CourtTransform, const FVector& World);

	/** 向きと大きさだけの量（速度など）の変換：コート局所（m 系）↔ ワールド（cm 系）。位置の移動は含まない。 */
	static FVector LocalMetersToWorldVector(const FTransform& CourtTransform, const FVector& LocalMeters);
	static FVector WorldToLocalMetersVector(const FTransform& CourtTransform, const FVector& World);
};
