#pragma once

#include "CoreMinimal.h"
#include "VisMeshComponentBase.h"
#include "IndirectExampleActor.generated.h"

UCLASS()
class UIndirectComponent : public UVisMeshComponentBase
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	int32 NumInstances = 1000; // 总数

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	int32 NumColumns = 10;    // 换行阈值

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	float XSpace = 100.0f;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	float YSpace = 100.0f;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	UMaterialInterface* Material;
	
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& BoundTransform) const override;

	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const override;\
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;	
#endif
};


UCLASS(Placeable)
class AIndirectExampleActor : public AActor
{
	GENERATED_UCLASS_BODY()

public:

	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, Category = "Procedural")
	UIndirectComponent* IndirectComponent;
};