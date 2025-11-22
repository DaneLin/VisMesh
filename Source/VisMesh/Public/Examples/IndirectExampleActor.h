#pragma once

#include "CoreMinimal.h"
#include "IndirectExampleActor.generated.h"

class UVisMeshIndirectComponent;

UCLASS(Placeable)
class AIndirectExampleActor : public AActor
{
	GENERATED_UCLASS_BODY()

public:

	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, Category = "Procedural")
	UVisMeshIndirectComponent* IndirectComponent;
};