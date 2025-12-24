// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RenderBase/VisMeshComponentBase.h"
#include "VisMeshInstancedComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class VISMESH_API UVisMeshInstancedComponent : public UVisMeshComponentBase
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	explicit UVisMeshInstancedComponent(FObjectInitializer const& Initializer);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	int32 NumInstances = 1; // 总数

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	int32 NumColumns = 1;    // 换行阈值

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	float XSpace = 100.0f;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	float YSpace = 100.0f;

	UPROPERTY(EditAnywhere, Category = "GenerationArgs")
	UMaterialInterface* Material;
	
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& BoundTransform) const override;

	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const override;
};
