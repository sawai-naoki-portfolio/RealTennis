#include "Stroke/TennisStrokeSelect.h"

#include "TennisSimLog.h"

EStrokeHeightBand TennisStrokeSelect::HeightBandOf(double BallHeightM)
{
	if (!FMath::IsFinite(BallHeightM))
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Select] HeightBandOf: invalid height (using Mid)"));
		return EStrokeHeightBand::Mid;
	}
	if (BallHeightM < VeryLowBelow)
	{
		return EStrokeHeightBand::VeryLow;
	}
	if (BallHeightM < LowBelow)
	{
		return EStrokeHeightBand::Low;
	}
	if (BallHeightM < MidBelow)
	{
		return EStrokeHeightBand::Mid;
	}
	return BallHeightM <= HighUpTo ? EStrokeHeightBand::High : EStrokeHeightBand::Overhead;
}

EStrokeFamily TennisStrokeSelect::FamilyOf(EHitKind Kind, ETennisShotType Type, double TimeBudgetS, double BlockTimeBudgetS)
{
	switch (Kind)
	{
	case EHitKind::Volley:
		return EStrokeFamily::Volley;
	case EHitKind::HalfVolley:
		return EStrokeFamily::HalfVolley;
	case EHitKind::Smash:
		return EStrokeFamily::Smash;
	case EHitKind::Groundstroke:
		break;
	}
	if (TimeBudgetS < BlockTimeBudgetS)
	{
		return EStrokeFamily::Block;
	}
	switch (Type)
	{
	case ETennisShotType::Flat:
	case ETennisShotType::Topspin:
	case ETennisShotType::HeavyTopspin:
		return EStrokeFamily::Drive;
	case ETennisShotType::Slice:
		return EStrokeFamily::Slice;
	case ETennisShotType::Lob:
	case ETennisShotType::SliceLob:
		return EStrokeFamily::Lob;
	case ETennisShotType::Drop:
		return EStrokeFamily::Drop;
	default:
		UE_LOG(LogTennisStroke, Warning, TEXT("[Select] FamilyOf: shot type %d does not fit a groundstroke (using Drive)"), static_cast<int32>(Type));
		return EStrokeFamily::Drive;
	}
}

EStrokeReach TennisStrokeSelect::ReachOf(bool bStretch)
{
	return bStretch ? EStrokeReach::Stretch : EStrokeReach::Normal;
}
