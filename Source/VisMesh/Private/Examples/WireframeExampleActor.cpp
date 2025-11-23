#include "Examples/WireframeExampleActor.h"
#include "Components/VisMeshLineComponent.h"

AWireframeExampleActor::AWireframeExampleActor(FObjectInitializer const& Initializer)
	: AActor(Initializer)
{
	LineComponent = CreateDefaultSubobject<UVisMeshLineComponent>(TEXT("VisMeshLineComponent"));
	SetRootComponent(LineComponent);
}

void AWireframeExampleActor::BeginPlay()
{
	Super::BeginPlay();
}






