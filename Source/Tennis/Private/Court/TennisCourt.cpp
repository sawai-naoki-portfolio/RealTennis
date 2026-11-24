#include "Court/TennisCourt.h"

#include "Components/SceneComponent.h"

ATennisCourt::ATennisCourt()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Origin"));  // ネット中央の地面
}

FVector ATennisCourt::LocalMetersToWorld(const FVector& LocalMeters) const
{
	return LocalMetersToWorld(GetActorTransform(), LocalMeters);
}

FVector ATennisCourt::WorldToLocalMeters(const FVector& World) const
{
	return WorldToLocalMeters(GetActorTransform(), World);
}

FVector ATennisCourt::LocalMetersToWorld(const FTransform& CourtTransform, const FVector& LocalMeters)
{
	return CourtTransform.TransformPositionNoScale(LocalMeters * UnrealUnitsPerMeter);
}

FVector ATennisCourt::WorldToLocalMeters(const FTransform& CourtTransform, const FVector& World)
{
	return CourtTransform.InverseTransformPositionNoScale(World) / UnrealUnitsPerMeter;
}

FVector ATennisCourt::LocalMetersToWorldVector(const FTransform& CourtTransform, const FVector& LocalMeters)
{
	return CourtTransform.TransformVectorNoScale(LocalMeters * UnrealUnitsPerMeter);
}

FVector ATennisCourt::WorldToLocalMetersVector(const FTransform& CourtTransform, const FVector& World)
{
	return CourtTransform.InverseTransformVectorNoScale(World) / UnrealUnitsPerMeter;
}
