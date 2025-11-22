#pragma once
#include "RenderBase/VisMeshSceneProxyBase.h"


/** Vis mesh scene proxy */
class FVisMeshProceduralSceneProxy : public FVisMeshSceneProxyBase
{
public:
	virtual SIZE_T GetTypeHash() const override;

	FVisMeshProceduralSceneProxy(UVisMeshProceduralComponent* Component);

	virtual ~FVisMeshProceduralSceneProxy() override;

	void UpdateSection_RenderThread(FRHICommandListBase& RHICmdList, FVisMeshSectionUpdateData* SectionData);

	void SetSectionVisibility_RenderThread(int32 SectionIndex, bool bNewVisibility);

	// 收集每个view下每个LOD的FPrimitiveSceneProxy，并转换成FMeshBatch
	// 设置FMeshBatch中的FMeshBatchElement中的IndexBuffer, NumPrimitive, UniformBuffer等等关于渲染的东西
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,uint32 VisibilityMap, class FMeshElementCollector& Collector) const override;

	// 用于确定View渲染的相关性，可以认为是MeshPass的第一层过滤，用于确定是否参与某些特性的绘制
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual bool CanBeOccluded() const override;

	virtual uint32 GetMemoryFootprint(void) const override;

	virtual void DispatchComputePass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily) override;

private:
	// Array of sections
	TArray<FVisMeshProxySection*> Sections;

	UBodySetup* BodySetup;

	FMaterialRelevance MaterialRelevance;
};