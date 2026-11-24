#include "Shot/ShotSpin.h"

#include "TennisSimLog.h"

double TennisShotSpin::AxisDeg(double RightHandedAxisDeg, ETennisHandedness Handedness, double SideInput)
{
	if (!FMath::IsFinite(SideInput))
	{
		UE_LOG(LogTennisShot, Warning, TEXT("[Spin] invalid SideInput=%f (using 0)"), SideInput);
		SideInput = 0.0;
	}
	const double BaseDeg = Handedness == ETennisHandedness::Left ? -RightHandedAxisDeg : RightHandedAxisDeg;
	return BaseDeg - MaxAxisTiltDeg * FMath::Clamp(SideInput, -1.0, 1.0);
}
