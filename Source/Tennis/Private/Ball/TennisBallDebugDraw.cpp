#include "Ball/TennisBallDebugDraw.h"

#include "Ball/TennisBallPredictor.h"
#include "Court/TennisCourt.h"
#include "DrawDebugHelpers.h"

namespace
{
	constexpr int32 PolylineStride = 4;      // 240 Hz の状態を 60 Hz に間引いて描く
	constexpr float LineThickness = 1.5f;    // 画素
	constexpr float MarkerRadius = 8.0f;     // cm
}

TArray<FVector> TennisBallDebugDraw::TrajectoryPolyline(const FTrajectory& Trajectory, const FTransform& CourtTransform, int32 Stride)
{
	const int32 Step = FMath::Max(Stride, 1);
	const TArray<FBallState>& States = Trajectory.States;
	TArray<FVector> Points;
	Points.Reserve(States.Num() / Step + 2);
	for (int32 Index = 0; Index < States.Num(); Index += Step)
	{
		Points.Add(ATennisCourt::LocalMetersToWorld(CourtTransform, States[Index].Pos));
	}
	if (!States.IsEmpty() && (States.Num() - 1) % Step != 0)
	{
		Points.Add(ATennisCourt::LocalMetersToWorld(CourtTransform, States.Last().Pos));
	}
	return Points;
}

void TennisBallDebugDraw::DrawPrediction(const UWorld* World, const ATennisCourt& Court, const FTrajectory& Trajectory)
{
#if ENABLE_DRAW_DEBUG
	const TArray<FVector> Points = TrajectoryPolyline(Trajectory, Court.GetActorTransform(), PolylineStride);
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		DrawDebugLine(World, Points[Index - 1], Points[Index], FColor::Yellow, false, -1.0f, 0, LineThickness);
	}
	for (const FBallEvent& Event : Trajectory.Events)
	{
		const FColor Color = Event.Type == EBallEventType::Bounce ? FColor::Red
			: Event.Type == EBallEventType::NetCross ? FColor::Green : FColor::Orange;
		DrawDebugSphere(World, Court.LocalMetersToWorld(Event.Pos), MarkerRadius, 8, Color, false, -1.0f, 0, LineThickness);
	}
#endif
}

void TennisBallDebugDraw::DrawActualPath(const UWorld* World, const ATennisCourt& Court, const TArray<FVector>& LocalPath)
{
#if ENABLE_DRAW_DEBUG
	for (int32 Index = 1; Index < LocalPath.Num(); ++Index)
	{
		DrawDebugLine(World, Court.LocalMetersToWorld(LocalPath[Index - 1]), Court.LocalMetersToWorld(LocalPath[Index]),
			FColor::Cyan, false, -1.0f, 0, LineThickness);
	}
#endif
}
