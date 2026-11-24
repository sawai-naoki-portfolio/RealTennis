#include "Core/TennisPlayerSettings.h"

#include "Data/ShotTypeData.h"

const UShotTypeData* UTennisPlayerSettings::FindShotType(ETennisShotType Type) const
{
	for (const TSoftObjectPtr<UShotTypeData>& Soft : ShotTypes)
	{
		const UShotTypeData* Data = Soft.LoadSynchronous();
		if (Data != nullptr && Data->Type == Type)
		{
			return Data;
		}
	}
	return nullptr;
}
