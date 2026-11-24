#include "Court/CourtGeometry.h"

#include "TennisConstants.h"

double TennisCourtGeometry::NetHeightAt(double Y)
{
	const double Along = FMath::Min(FMath::Abs(Y), TennisConst::SinglesStickY);
	return TennisConst::NetHeightCenter
		+ (TennisConst::NetHeightPost - TennisConst::NetHeightCenter) * Along / TennisConst::SinglesStickY;
}

bool TennisCourtGeometry::HasNetAt(double Y)
{
	return FMath::Abs(Y) <= TennisConst::NetPostY;
}

bool TennisCourtGeometry::HasNetBodyAt(double Y)
{
	return FMath::Abs(Y) <= TennisConst::SinglesStickY;
}
