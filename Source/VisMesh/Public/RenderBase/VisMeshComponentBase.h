#pragma once

#include "CoreMinimal.h"

#include "VisMeshComponentBase.generated.h"

UCLASS(Abstract)
class UVisMeshComponentBase : public UMeshComponent 
{
	GENERATED_BODY()
public:
	UVisMeshComponentBase(const FObjectInitializer& ObjectInitializer);
	
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;	
#endif
};