#pragma once
#include "RenderBase/VisMeshComponentBase.h"
#include "VisMeshLineComponent.generated.h"

UCLASS()
class UVisMeshLineComponent : public UVisMeshComponentBase
{
	GENERATED_BODY()
public:

	explicit UVisMeshLineComponent(const FObjectInitializer& ObjectInitializer);
	
	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	int32 NumInstances = 1000; // 总数

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	int32 NumColumns = 10;    // 换行阈值

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	float XSpace = 100.0f;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	float YSpace = 100.0f;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	float LineWidth = 1.0f;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	UMaterialInterface* Material;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	bool bEnableMiter = false;
	
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& BoundTransform) const override;

	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const override;
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;	
#endif
};