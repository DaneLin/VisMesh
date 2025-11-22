#include "Examples/IndirectExampleActor.h"
#include "Components/VisMeshIndirectComponent.h"

AIndirectExampleActor::AIndirectExampleActor(FObjectInitializer const& Initializer)
	: AActor(Initializer)
{
	IndirectComponent = CreateDefaultSubobject<UVisMeshIndirectComponent>(TEXT("IndirectComponent"));
	SetRootComponent(IndirectComponent);
}

void AIndirectExampleActor::BeginPlay()
{
	Super::BeginPlay();
}






