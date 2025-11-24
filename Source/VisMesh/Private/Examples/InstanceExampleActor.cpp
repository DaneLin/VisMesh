#include "Examples/InstanceExampleActor.h"
#include "Components/VisMeshInstancedComponent.h"

AInstanceExampleActor::AInstanceExampleActor(FObjectInitializer const& Initializer)
	: AActor(Initializer)
{
	InstanceComponent = CreateDefaultSubobject<UVisMeshInstancedComponent>(TEXT("VisMeshInstancedComponent"));
	SetRootComponent(InstanceComponent);
}

void AInstanceExampleActor::BeginPlay()
{
	Super::BeginPlay();
}






