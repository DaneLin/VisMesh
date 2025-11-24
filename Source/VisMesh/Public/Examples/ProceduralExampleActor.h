#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/VisMeshProceduralComponent.h"
#include "ProceduralExampleActor.generated.h"

UCLASS()
class AProceduralExampleActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AProceduralExampleActor();

protected:
	virtual void BeginPlay() override;

public:
	// 声明组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VisMesh")
	TObjectPtr<UVisMeshProceduralComponent> VisMeshComp;
};