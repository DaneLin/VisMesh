#pragma once

#include "CoreMinimal.h"
#include "InstanceExampleActor.generated.h"

class UVisMeshInstancedComponent;

UCLASS(Placeable)
class AInstanceExampleActor : public AActor
{
	GENERATED_UCLASS_BODY()

public:

	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, Category = "Procedural")
	UVisMeshInstancedComponent* InstanceComponent;
};