#pragma once
#include "RenderBase/VisMeshComponentBase.h"
#include "VisMeshIndirectComponent.generated.h"

UCLASS()
class UVisMeshIndirectComponent : public UVisMeshComponentBase
{
	GENERATED_BODY()
public:

	explicit UVisMeshIndirectComponent(const FObjectInitializer& ObjectInitializer);
	
	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	int32 NumInstances = 1000; // 总数

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	int32 NumColumns = 10;    // 换行阈值

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	float XSpace = 100.0f;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	float YSpace = 100.0f;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	FVector3f BoundsMin = FVector3f::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	FVector3f BoundsMax = FVector3f(10, 10, 10);

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	float Radius = 10.f;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	int NumPoints = 10;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	bool bDrawSphere;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	bool bUpdateEverTick = false;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	UMaterialInterface* Material;
	
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& BoundTransform) const override;

	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const override;
	

};