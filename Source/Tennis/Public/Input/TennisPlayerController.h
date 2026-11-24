// 選手の操作（操作仕様 §2・§3、DD-03 §2.2）：Enhanced Input の Context を加え、移動の入力をポーンに渡す。
// 移動の向きはコートに対して決める（上＝ネットの方向。DD-03 §2.2）。球種の入力は UTennisShotInputComponent に任せる。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TennisPlayerController.generated.h"

struct FInputActionValue;
class UTennisShotInputComponent;
class UInputMappingContext;
enum class ETennisInputPhase : uint8;

UCLASS()
class TENNIS_API ATennisPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATennisPlayerController();

	UTennisShotInputComponent* GetShotInput() const { return ShotInput; }

	/** 試合進行がローカル選手の操作段階を指定する。未設定の段階なら現在の割り当てを保つ。 */
	bool ApplyInputPhase(ETennisInputPhase Phase);

	/** スティックの入力（x：右、y：上）を、上を ForwardYawDegrees（度）の向きとしたワールドの水平の向きにする。長さは 1 以下に収める。 */
	static FVector StickToWorldDirection(const FVector2D& Stick, double ForwardYawDegrees);

	/** 選手の側（Side：−1 は X < 0、+1 は X > 0）から見てネットの方向の Yaw（度）。コートの Yaw に、X > 0 の側なら 180° を足す */
	static double NetwardYawDegrees(double CourtYawDegrees, double Side);

protected:
	virtual void ReceivedPlayer() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	TOptional<ETennisInputPhase> AppliedPhase;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> ActiveContext;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> ActiveGlobalContext;

	void OnMove(const FInputActionValue& Value);

	UPROPERTY(VisibleAnywhere, Category = "Input")
	TObjectPtr<UTennisShotInputComponent> ShotInput;
};
