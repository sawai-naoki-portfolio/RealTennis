// 操作のアセットの参照と入力の値（Project Settings → Game → Tennis Input。Config/DefaultGame.ini にコミットする）。
// C++ にアセットのパスを書かないため、Input Action と Mapping Context はここで指す（操作仕様 §3）。
#pragma once

#include "Engine/DeveloperSettings.h"
#include "Input/TennisShotInputMachine.h"
#include "Shot/ShotTypes.h"
#include "TennisInputSettings.generated.h"

class UInputAction;
class UInputMappingContext;

/** M3 で使う段階（SPEC-04 §3.2）。Menu では Global だけを残す。 */
UENUM(BlueprintType)
enum class ETennisInputPhase : uint8
{
	PreServe, Serving, Receive, Rally, Menu
};

USTRUCT()
struct FTennisPhaseContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Contexts")
	TSoftObjectPtr<UInputMappingContext> Context;

	UPROPERTY(EditAnywhere, Category = "Contexts")
	int32 Priority = 10;
};

/** 球種のボタン 1 つ：Input Action と、球種（修飾なし・技の修飾つき）。操作仕様 §3.1 */
USTRUCT()
struct FTennisShotButton
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Shot")
	TSoftObjectPtr<UInputAction> Action;

	UPROPERTY(Config, EditAnywhere, Category = "Shot")
	ETennisShotType Type = ETennisShotType::Topspin;

	/** 技の修飾（IA_Mod_Technique）を押しているときの球種 */
	UPROPERTY(Config, EditAnywhere, Category = "Shot")
	ETennisShotType TechniqueType = ETennisShotType::Topspin;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Tennis Input"))
class TENNIS_API UTennisInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Contexts")
	FTennisPhaseContext GlobalContext;

	/** サーブ準備・トス中・レシーブ準備の Context。ラリーは既存の設定を使う。 */
	UPROPERTY(Config, EditAnywhere, Category = "Contexts")
	TMap<ETennisInputPhase, FTennisPhaseContext> PhaseContexts;

	/** ラリー中の操作（IMC_Rally：移動・球種など）。優先度は操作仕様 §3.2 の 10 */
	UPROPERTY(Config, EditAnywhere, Category = "Contexts")
	TSoftObjectPtr<UInputMappingContext> RallyContext;

	UPROPERTY(Config, EditAnywhere, Category = "Contexts")
	int32 RallyContextPriority = 10;

	/** 移動（IA_Move、Axis2D。x：右、y：上） */
	UPROPERTY(Config, EditAnywhere, Category = "Actions")
	TSoftObjectPtr<UInputAction> MoveAction;

	/** ラリーの球種のボタン（IA_Shot_*）。並びの番号が、状態機械のボタンの番号になる */
	UPROPERTY(Config, EditAnywhere, Category = "Actions")
	TArray<FTennisShotButton> RallyShotButtons;

	/** 技の修飾（IA_Mod_Technique、Bool）：ドロップ・スライスのロブ */
	UPROPERTY(Config, EditAnywhere, Category = "Actions")
	TSoftObjectPtr<UInputAction> TechniqueAction;

	/** 入力バッファ（DATA-02 §5）：打点の候補が見つからない予約を、押してから何秒で捨てるか */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0.1", ClampMax = "3.0", Units = "s"))
	double InputBufferSeconds = FTennisShotInputMachine::DefaultBufferSeconds;
};
