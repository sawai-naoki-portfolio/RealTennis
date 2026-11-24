// 試合のゲームモード（ARCH-01）。今は選手のポーンとコントローラーの既定だけを決める。
// 試合の流れ（段階・ポイント・スコア）は M5 で加える。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TennisGameMode.generated.h"

UCLASS()
class TENNIS_API ATennisGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATennisGameMode();
};
