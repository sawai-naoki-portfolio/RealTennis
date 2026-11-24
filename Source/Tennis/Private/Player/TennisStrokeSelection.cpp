#include "Player/TennisStrokeSelection.h"

#include "Chooser.h"
#include "Data/StrokeData.h"
#include "IObjectChooser.h"
#include "TennisSimLog.h"

FTennisStrokeChooserInput TennisStrokeSelection::MakeInput(const FHitCandidate& Candidate, ETennisShotType ShotType,
	EBackhandStyle BackhandStyle, double NowSeconds, double BlockTimeBudgetSeconds)
{
	FTennisStrokeChooserInput Input;
	Input.Side = Candidate.Side;
	Input.BackhandStyle = BackhandStyle;
	Input.Height = TennisStrokeSelect::HeightBandOf(Candidate.BallPos.Z);
	Input.HitHeight = Candidate.BallPos.Z;
	Input.Reach = TennisStrokeSelect::ReachOf(Candidate.bStretch);
	Input.TimeBudget = static_cast<float>(FMath::Max(Candidate.Time - NowSeconds, 0.0));
	Input.Family = TennisStrokeSelect::FamilyOf(Candidate.Kind, ShotType, Input.TimeBudget, BlockTimeBudgetSeconds);
	return Input;
}

FTennisStrokeChooserInput TennisStrokeSelection::MidDriveInput(EStrokeSide Side, EBackhandStyle BackhandStyle)
{
	FTennisStrokeChooserInput Input;
	Input.Side = Side;
	Input.BackhandStyle = BackhandStyle;
	Input.Height = EStrokeHeightBand::Mid;
	Input.HitHeight = MidDriveHitHeight;
	Input.Reach = EStrokeReach::Normal;
	Input.Family = EStrokeFamily::Drive;
	return Input;
}

UStrokeData* TennisStrokeSelection::Select(const UChooserTable* Table, FTennisStrokeChooserInput& Input)
{
	if (Table == nullptr)
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Select] no chooser table"));
		return nullptr;
	}
	FChooserEvaluationContext Context;
	Context.AddStructParam(Input);  // 表の Parameters の 0 番（FTennisStrokeChooserInput）
	UStrokeData* Result = nullptr;
	UChooserTable::EvaluateChooser(Context, Table, FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda(
		[&Result](UObject* Object)
		{
			Result = Cast<UStrokeData>(Object);
			return Result != nullptr ? FObjectChooserBase::EIteratorStatus::Stop : FObjectChooserBase::EIteratorStatus::Failed;
		}));
	if (Result == nullptr)
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Select] %s: no stroke for side=%d backhand=%d height=%d reach=%d family=%d stance=%d"),
			*Table->GetName(), static_cast<int32>(Input.Side), static_cast<int32>(Input.BackhandStyle), static_cast<int32>(Input.Height),
			static_cast<int32>(Input.Reach), static_cast<int32>(Input.Family), static_cast<int32>(Input.Stance));
	}
	return Result;
}
