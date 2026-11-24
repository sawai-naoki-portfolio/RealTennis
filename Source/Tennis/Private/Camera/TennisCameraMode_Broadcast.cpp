#include "Camera/TennisCameraMode_Broadcast.h"

#include "Court/TennisCourt.h"
#include "TennisGameLog.h"

FMinimalViewInfo UTennisCameraMode_Broadcast::Evaluate(const FTennisCameraContext& Context, float DeltaSeconds)
{
	FVector DesiredLocation;
	FVector DesiredLookAt;
	TennisBroadcastCamera::Desired(Params, Context.Side, Context.Player, Context.Ball, DesiredLocation, DesiredLookAt);

	if (LastSide != Context.Side)  // 始まり・側が替わった（エンドチェンジの回り込みは W-5.10）：飛んでこないように目標にそろえる
	{
		Location.Reset(DesiredLocation);
		LookAt.Reset(DesiredLookAt);
		LastSide = Context.Side;
		UE_LOG(LogTennisCamera, Log, TEXT("[Camera] broadcast: side=%+.0f aspect=%.3f (snapped to the target)"), Context.Side, Context.ViewAspect);
	}
	else
	{
		Location.Step(DesiredLocation, Params.LocationOmega, DeltaSeconds);
		LookAt.Step(DesiredLookAt, Params.LookAtOmega, DeltaSeconds);
	}

	double FovDeg = Params.FovDeg;
	const FRotator LocalRotation = TennisBroadcastCamera::ConstrainedRotation(
		Params, Context.Side, Context.ViewAspect, Location.Value, LookAt.Value, Context.Player, FovDeg);

	FMinimalViewInfo View;
	View.Location = ATennisCourt::LocalMetersToWorld(Context.CourtTransform, Location.Value);
	View.Rotation = (Context.CourtTransform.GetRotation() * LocalRotation.Quaternion()).Rotator();
	View.FOV = static_cast<float>(FovDeg);
	View.AspectRatio = static_cast<float>(Params.ReferenceAspect);
	return View;
}

void UTennisCameraMode_Broadcast::Reset()
{
	LastSide = 0.0;
}
