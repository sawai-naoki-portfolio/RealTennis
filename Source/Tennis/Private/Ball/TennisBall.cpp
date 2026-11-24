#include "Ball/TennisBall.h"

#include "Ball/TennisBallSimComponent.h"
#include "Components/StaticMeshComponent.h"

ATennisBall::ATennisBall()
{
	PrimaryActorTick.bCanEverTick = false;  // 動きは Sim のコンポーネントが進める

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // 当たりは自前の物理（DD-01）
	Mesh->SetMobility(EComponentMobility::Movable);
	RootComponent = Mesh;

	Sim = CreateDefaultSubobject<UTennisBallSimComponent>(TEXT("Sim"));
}
