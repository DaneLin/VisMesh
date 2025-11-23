#include "Components/VisMeshLineComponent.h"

#include "MaterialDomain.h"
#include "Components/VisMeshLineSceneProxy.h"

UVisMeshLineComponent::UVisMeshLineComponent(FObjectInitializer const& Initializer)
	: Super(Initializer)
{
	if(Material == nullptr)
	{
		Material = UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface);
	}
}

FPrimitiveSceneProxy* UVisMeshLineComponent::CreateSceneProxy()
{
	return new FVisMeshLineSceneProxy(this);
}


FBoxSphereBounds UVisMeshLineComponent::CalcBounds(const FTransform& BoundTransform) const
{
	// 返回一个极其巨大的 Bounds，确保 CPU 永远认为它可见
	// HALF_WORLD_MAX 大约是 UE 世界的一半大小
	return FBoxSphereBounds(FVector::ZeroVector, FVector(HALF_WORLD_MAX), HALF_WORLD_MAX);
}

void UVisMeshLineComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if(Material != nullptr)
	{
		OutMaterials.Add(Material);
	}
}

void UVisMeshLineComponent::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	MarkRenderStateDirty();
}