#include "Core/TennisGameMode.h"

#include "Input/TennisPlayerController.h"
#include "Player/TennisCharacter.h"

ATennisGameMode::ATennisGameMode()
{
	DefaultPawnClass = ATennisCharacter::StaticClass();
	PlayerControllerClass = ATennisPlayerController::StaticClass();
}
