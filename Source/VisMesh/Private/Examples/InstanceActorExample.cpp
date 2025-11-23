#include "Examples/InstanceActorExample.h"
#include "Components/VisMeshInstancedComponent.h"

AInstanceActorExample::AInstanceActorExample(FObjectInitializer const& Initializer)
	: AActor(Initializer)
{
	InstanceComponent = CreateDefaultSubobject<UVisMeshInstancedComponent>(TEXT("VisMeshInstancedComponent"));
	SetRootComponent(InstanceComponent);
}

void AInstanceActorExample::BeginPlay()
{
	Super::BeginPlay();
}






