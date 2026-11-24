#include "Core/TennisDebugSettings.h"
#include "Modules/ModuleManager.h"

class FTennisModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		UTennisDebugSettings::RegisterConsoleCommands();
	}

	virtual void ShutdownModule() override
	{
		UTennisDebugSettings::UnregisterConsoleCommands();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FTennisModule, Tennis, "Tennis");
