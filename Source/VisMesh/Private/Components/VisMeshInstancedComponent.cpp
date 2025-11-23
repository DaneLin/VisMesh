// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/VisMeshInstancedComponent.h"

#include "MaterialDomain.h"
#include "Components/VisMeshInstancedSceneProxy.h"


// Sets default values for this component's properties
UVisMeshInstancedComponent::UVisMeshInstancedComponent(FObjectInitializer const& Initializer)
	:Super(Initializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
	if(Material == nullptr)
	{
		Material = UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface);
	}
}


// Called when the game starts
void UVisMeshInstancedComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UVisMeshInstancedComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                               FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

FPrimitiveSceneProxy* UVisMeshInstancedComponent::CreateSceneProxy()
{
	return new FVisMeshInstancedSceneProxy(this);
}

FBoxSphereBounds UVisMeshInstancedComponent::CalcBounds(const FTransform& BoundTransform) const
{
	return FBoxSphereBounds(FVector::ZeroVector, FVector(HALF_WORLD_MAX), HALF_WORLD_MAX);

}

void UVisMeshInstancedComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials,
	bool bGetDebugMaterials) const
{
	if(Material != nullptr)
	{
		OutMaterials.Add(Material);
	}
}

void UVisMeshInstancedComponent::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	MarkRenderStateDirty();
}

