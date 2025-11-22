#pragma once
#include "RenderBase/VisMeshSceneProxyBase.h"

class FVisMeshIndirectSceneProxy final : public FVisMeshSceneProxyBase
{
public:
	FLocalVertexFactory* VertexFactory;
	FPositionUAVVertexBuffer* PositionBuffer;
	TRefCountPtr<FRHIBuffer> IndirectArgsBuffer;
	FUnorderedAccessViewRHIRef IndirectArgsBufferUAV;

	float XSpace, YSpace;
	int32 NumColumns, NumInstances;

	UMaterialInterface* Material;

public:
	FVisMeshIndirectSceneProxy(UVisMeshIndirectComponent* Owner) ;

	virtual SIZE_T GetTypeHash() const override;

	virtual uint32 GetMemoryFootprint(void) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual void CreateRenderThreadResources() override;

	virtual void DestroyRenderThreadResources() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const override;

	FMeshBatch* CreateMeshBatch(class FMeshElementCollector& Collector) const;
	
	virtual void DispatchComputePass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily) override;
};
