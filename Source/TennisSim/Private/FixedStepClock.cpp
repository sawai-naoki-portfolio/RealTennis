#include "FixedStepClock.h"

#include "TennisConstants.h"

int32 FFixedStepClock::Advance(double FrameTime, double TimeScale)
{
	const double Frame = FrameTime > 0.0 ? FMath::Min(FrameTime, MaxFrameTime) : 0.0;  // NaN も 0
	const double Scale = TimeScale > 0.0 ? TimeScale : 0.0;
	Accumulator += Frame * Scale;

	int32 Steps = 0;
	while (Accumulator >= TennisConst::SimDt)
	{
		Accumulator -= TennisConst::SimDt;
		++Steps;
	}
	return Steps;
}

double FFixedStepClock::Alpha() const
{
	return FMath::Clamp(Accumulator / TennisConst::SimDt, 0.0, 1.0 - UE_DOUBLE_KINDA_SMALL_NUMBER);
}
