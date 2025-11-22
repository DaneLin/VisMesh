#include "Examples/IndirectExampleActor.h"

#include "GlobalShader.h"
#include "LocalVertexFactory.h"
#include "MaterialDomain.h"
#include "RenderResource.h"
#include "RHI.h"
#include "RHIUtilities.h"
#include "VisMeshSceneProxy.h"
#include "VisMeshSubsystem.h"

UIndirectComponent::UIndirectComponent(FObjectInitializer const& Initializer)
	: UVisMeshComponentBase(Initializer)
{
	if(Material == nullptr)
	{
		Material = UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface);
	}
}

FPrimitiveSceneProxy* UIndirectComponent::CreateSceneProxy()
{
	return new FVisMeshIndirectSceneProxy(this);
}


FBoxSphereBounds UIndirectComponent::CalcBounds(const FTransform& BoundTransform) const
{
	// 返回一个极其巨大的 Bounds，确保 CPU 永远认为它可见
	// HALF_WORLD_MAX 大约是 UE 世界的一半大小
	return FBoxSphereBounds(FVector::ZeroVector, FVector(HALF_WORLD_MAX), HALF_WORLD_MAX);
}

void UIndirectComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if(Material != nullptr)
	{
		OutMaterials.Add(Material);
	}
}

void UIndirectComponent::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	MarkRenderStateDirty();
}

AIndirectExampleActor::AIndirectExampleActor(FObjectInitializer const& Initializer)
	: AActor(Initializer)
{
	IndirectComponent = CreateDefaultSubobject<UIndirectComponent>(TEXT("IndirectComponent"));
	SetRootComponent(IndirectComponent);
}

void AIndirectExampleActor::BeginPlay()
{
	Super::BeginPlay();
}






