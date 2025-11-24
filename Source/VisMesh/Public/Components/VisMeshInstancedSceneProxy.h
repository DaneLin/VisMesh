#pragma once
#include "RenderBase/VisMeshSceneProxyBase.h"

class FVisMeshInstanceBuffer;
class FVisMeshInstancedVertexFactory;
class UVisMeshInstancedComponent;

class FVisMeshInstancedSceneProxy final : public FVisMeshSceneProxyBase
{
public:
	FVisMeshInstancedVertexFactory* VertexFactory;
	FPositionUAVVertexBuffer* PositionBuffer;
	TRefCountPtr<FRDGPooledBuffer> IndirectArgsBuffer;
	FUnorderedAccessViewRHIRef IndirectArgsBufferUAV;

	FVisMeshInstanceBuffer* InstanceBuffer;

	float XSpace, YSpace;
	int32 NumColumns, NumInstances;

	UMaterialInterface* Material;

public:
	FVisMeshInstancedSceneProxy(UVisMeshInstancedComponent* Owner) ;

	virtual SIZE_T GetTypeHash() const override;

	virtual uint32 GetMemoryFootprint(void) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual void CreateRenderThreadResources() override;

	virtual void DestroyRenderThreadResources() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const override;

	FMeshBatch* CreateMeshBatch(class FMeshElementCollector& Collector) const;
	
	virtual void DispatchComputePass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily) override;
};
