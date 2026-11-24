#include "Player/TennisRacketComponent.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "TennisSimLog.h"

const FName UTennisRacketComponent::GripSocket(TEXT("grip"));
const FName UTennisRacketComponent::SweetSpotSocket(TEXT("racket_sweetspot"));

UTennisRacketComponent::UTennisRacketComponent()
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision);  // 打球はスイートスポットとボールの距離で決める（ボールは自前の物理）
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
}

bool UTennisRacketComponent::SetRacketMesh(UStaticMesh* Mesh)
{
	const UStaticMeshSocket* Grip = Mesh != nullptr ? Mesh->FindSocket(GripSocket) : nullptr;
	if (Grip == nullptr)
	{
		UE_LOG(LogTennisStroke, Warning, TEXT("[Racket] %s: no racket mesh or no '%s' socket (racket=%s)"),
			*GetNameSafe(GetOwner()), *GripSocket.ToString(), *GetNameSafe(Mesh));
		return false;
	}
	SetStaticMesh(Mesh);
	// ラケットの局所座標で grip にある点を、親のソケットの原点に重ねる（向きも grip の軸を親のソケットの軸に合わせる）
	SetRelativeTransform(FTransform(Grip->RelativeRotation, Grip->RelativeLocation, Grip->RelativeScale).Inverse());
	return true;
}

FVector UTennisRacketComponent::GetSweetSpotWorld() const
{
	return GetSocketLocation(SweetSpotSocket);
}
