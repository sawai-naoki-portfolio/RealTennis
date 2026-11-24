#include "Player/TennisPlayerAppearance.h"

#include "Data/StrokeData.h"

bool FTennisReachBody::IsBaked() const
{
	const double Values[] = { Forward, ForehandSide, OneHandedBackhandSide, TwoHandedBackhandSide };
	for (const double Value : Values)
	{
		if (!FMath::IsFinite(Value) || Value <= 0.0)
		{
			return false;
		}
	}
	return true;
}

const FTennisStrokeFit* FTennisStrokeFit::Find(const TArray<FTennisStrokeFit>& Fits, const UStrokeData* Stroke)
{
	if (Stroke == nullptr)
	{
		return nullptr;
	}
	const FSoftObjectPath Path(Stroke);
	return Fits.FindByPredicate([&Path](const FTennisStrokeFit& Fit) { return Fit.Stroke.ToSoftObjectPath() == Path; });
}

const FTennisStrokeFit* UTennisPlayerAppearance::FindStrokeFit(const UStrokeData* Stroke) const
{
	return FTennisStrokeFit::Find(StrokeFits, Stroke);
}
