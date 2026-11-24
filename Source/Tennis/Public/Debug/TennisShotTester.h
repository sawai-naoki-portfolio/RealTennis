// 打球ツール（FR-DBG-003、ARCH-02 §3）：PIE で、クリックしたコートの地点へ、選んだ球種で打つ。テストマップに置く開発用のアクタ。
// 打点・溜めは詳細パネルで決め、Tab で球種を切り替える。MCP からは ShootWith を呼ぶ（Tools/Unreal の shoot_ball）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shot/ShotTypes.h"
#include "TennisShotTester.generated.h"

class ATennisBall;
class UShotTypeData;
class UTennisBallSimComponent;
struct FShotTypeParams;

UCLASS()
class TENNIS_API ATennisShotTester : public AActor
{
	GENERATED_BODY()

public:
	ATennisShotTester();

	/** コート局所の狙い TargetLocal（m。Z は使わない）へ、今の球種で打つ。解けたら true（解けなくても、残差が最小の解で打つ）。 */
	UFUNCTION(BlueprintCallable, Category = "Tennis|Debug")
	bool ShootAt(FVector TargetLocal);

	/** 球種 Type を指定して打つ（MCP の shoot_ball が使う）。ボールか球種がなければ、警告を出して false。 */
	UFUNCTION(BlueprintCallable, Category = "Tennis|Debug")
	bool ShootWith(const UShotTypeData* Type, FVector TargetLocal);

	/** 次の球種に切り替える（最後の次は最初）。 */
	UFUNCTION(BlueprintCallable, Category = "Tennis|Debug")
	void NextShotType();

	/** 打点 HitPoint から Target へ（m、コート局所）、球種 Params・溜め Charge で解いて Sim から打ち出す。物理の条件は Sim と同じ。 */
	static FShotSolveResult Shoot(UTennisBallSimComponent& Sim, const FShotTypeParams& Params, const FVector& HitPoint, const FVector& Target,
		double Charge);

	/** コート局所の半直線（始点 Start、向き Direction）が地面（Z = 0）と交わる点。地面へ向かわないなら値なし。 */
	static TOptional<FVector> GroundPointOnRay(const FVector& Start, const FVector& Direction);

protected:
	virtual void BeginPlay() override;

	/** 打つボール。空ならプレイの開始時にレベルの中から探す。 */
	UPROPERTY(EditInstanceOnly, Category = "Tennis|Debug")
	TObjectPtr<ATennisBall> Ball;

	/** 切り替えられる球種（Tab で順に）。 */
	UPROPERTY(EditAnywhere, Category = "Tennis|Debug")
	TArray<TObjectPtr<UShotTypeData>> ShotTypes;

	/** 打点（m、コート局所）。 */
	UPROPERTY(EditAnywhere, Category = "Tennis|Debug")
	FVector HitPoint = FVector(-11.0, 0.0, 1.0);

	/** 溜め（0〜1）。 */
	UPROPERTY(EditAnywhere, Category = "Tennis|Debug", meta = (ClampMin = "0", ClampMax = "1"))
	double Charge = 0.5;

private:
	void OnClick();
	const UShotTypeData* CurrentShotType() const;
	void ShowCurrentShotType() const;

	int32 ShotIndex = 0;
};
