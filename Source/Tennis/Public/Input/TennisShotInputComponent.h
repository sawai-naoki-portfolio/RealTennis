// 球種の入力（操作仕様 §3.1・§4.1、ST-05、FR-INP-003）。コントローラーが持ち、Enhanced Input の球種のボタンと技の修飾を
// 状態機械（FTennisShotInputMachine）に渡す。意図が変わったら、その場で選手（ATennisCharacter）に渡す（T-INP-002：1 フレーム以内）。
// 選手の「打点の候補が見つかった」「ストロークが終わった」を購読して、状態機械を進める（BindCharacter）。インパクトは W-3.5 でつなぐ。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/TennisShotInputMachine.h"
#include "TennisShotInputComponent.generated.h"

class ATennisCharacter;
class UEnhancedInputComponent;

UCLASS()
class TENNIS_API UTennisShotInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTennisShotInputComponent();

	/** 設定（UTennisInputSettings）のボタンと修飾を Enhanced Input につなぎ、入力バッファの長さを決める */
	void BindInput(UEnhancedInputComponent& Input);

	void SetRallyInputEnabled(bool bEnabled);
	void OnShotPressed(int32 Button);
	void OnShotReleased(int32 Button);
	void OnTechnique(bool bHeld);
	/** 取り消し（ポーンの変更・入力の段階の変更） */
	void Cancel();
	/** 操作する選手の出来事を購読する（前の選手の購読は外す。nullptr なら外すだけ） */
	void BindCharacter(ATennisCharacter* Character);

	EShotInputState GetState() const { return Machine.GetState(); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool bRallyInputEnabled = true;
	/** 変わったら、選手に意図を渡す（待機に戻ったら取り消す） */
	void Publish(bool bChanged, const TCHAR* What);
	void OnCandidateFound();
	void OnStrokeEnded();
	void OnStrokeImpact();
	ATennisCharacter* GetCharacter() const;
	double GetNow() const;

	FTennisShotInputMachine  Machine;
	TArray<FShotButtonTypes> ButtonTypes;  // 番号は UTennisInputSettings::RallyShotButtons の並び
	TWeakObjectPtr<ATennisCharacter> BoundCharacter;
	FDelegateHandle CandidateHandle;
	FDelegateHandle StrokeEndedHandle;
	FDelegateHandle ImpactHandle;
};
