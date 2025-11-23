#pragma once

#include "CoreMinimal.h"
#include "InstanceActorExample.generated.h"

class UVisMeshInstancedComponent;

UCLASS(Placeable)
class AInstanceActorExample : public AActor
{
	GENERATED_UCLASS_BODY()

public:

	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, Category = "Procedural")
	UVisMeshInstancedComponent* InstanceComponent;
};