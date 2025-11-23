#pragma once

#include "CoreMinimal.h"
#include "WireframeExampleActor.generated.h"

class UVisMeshLineComponent;

UCLASS(Placeable)
class AWireframeExampleActor : public AActor
{
	GENERATED_UCLASS_BODY()

public:

	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, Category = "Procedural")
	UVisMeshLineComponent* LineComponent;
};