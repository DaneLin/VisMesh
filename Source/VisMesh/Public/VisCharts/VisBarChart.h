// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RenderBase/VisMeshRenderResources.h"
#include "VisBarChart.generated.h"

class UVisMeshProceduralComponent;

UCLASS()
class VISMESH_API AVisBarChart : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AVisBarChart();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VisMesh")
	UVisMeshProceduralComponent* MainMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VisMesh")
	UVisMeshProceduralComponent* HighlightMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VisMesh")
	UVisMeshProceduralComponent* SelectionMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VisMesh")
	UMaterialInterface* MainMeshMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VisMesh")
	UMaterialInterface* HighlightMeshMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisMesh")
	UMaterialInterface* SelectionMeshMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VisMesh")
	UMaterialParameterCollection* ChartMPC;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chart Config")
	float BarWidth = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chart Config")
	float BarGap = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chart Config")
	float HeightMultiplier = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chart Config")
	FLinearColor BaseColor = FLinearColor::Blue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chart Config")
	FLinearColor TopColor = FLinearColor::Gray;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chart Config")
	FLinearColor HoverColor = FLinearColor::Yellow; // 高亮颜色

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chart Config")
	float HoverScale = 1.5f;

	// 网格布局列数
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chart Runtime")
	int32 GridColumnCount = 1;

	/** 生成图表 */
	UFUNCTION(BlueprintCallable, Category = "Chart")
	void GenerateBarChart(const TArray<float>& DataValues);

private:
	TArray<float> CachedDataValues;
	
	TArray<FVector> TemplateVerts;

	int32 LastHoverIndex = -1;

	int32 SelectedIndex = -1;

	void HandleClick();

	int32 RaycastOnBarChart(FVector LocalStart, FVector LocalDir) const;

	void GenerateSelectionFrame(float Width, float Height, float Thickness);
};
