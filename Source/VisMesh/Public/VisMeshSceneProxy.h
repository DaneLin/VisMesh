#pragma once

#include "PrimitiveSceneProxy.h"
#include "VisMeshRenderResources.h"

class UVisMeshProceduralComponent;
class FVisMeshSectionUpdateData;
class FVisMeshProxySection;
class UIndirectComponent;
class FPositionUAVVertexBuffer;

DECLARE_STATS_GROUP(TEXT("VisMesh"), STATGROUP_VisMesh, STATCAT_Advanced);

// Cycle 统计项
DECLARE_CYCLE_STAT(TEXT("Create VisMesh Proxy"), STAT_VisMesh_CreateSceneProxy, STATGROUP_VisMesh);
DECLARE_CYCLE_STAT(TEXT("Create Mesh Section"), STAT_VisMesh_CreateMeshSection, STATGROUP_VisMesh);
DECLARE_CYCLE_STAT(TEXT("UpdateSection GT"), STAT_VisMesh_UpdateSectionGT, STATGROUP_VisMesh);
DECLARE_CYCLE_STAT(TEXT("UpdateSection RT"), STAT_VisMesh_UpdateSectionRT, STATGROUP_VisMesh);
DECLARE_CYCLE_STAT(TEXT("Get VisMesh Elements"), STAT_VisMesh_GetMeshElements, STATGROUP_VisMesh);
DECLARE_CYCLE_STAT(TEXT("Update Collision"), STAT_VisMesh_UpdateCollision, STATGROUP_VisMesh);

DEFINE_LOG_CATEGORY_STATIC(LogVisComponent, Log, All);

class FVisMeshBaseSceneProxy : public FPrimitiveSceneProxy
{
public:
	// 继承父类构造函数
	FVisMeshBaseSceneProxy(const UPrimitiveComponent* InComponent, FName ResourceName = NAME_None)
		: FPrimitiveSceneProxy(InComponent, ResourceName)
	{}

	// 这是我们要调用的纯虚函数
	virtual void DispatchComputePass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily) = 0;
};

class FVisMeshIndirectSceneProxy final : public FVisMeshBaseSceneProxy
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
	FVisMeshIndirectSceneProxy(UIndirectComponent* Owner) ;

	virtual SIZE_T GetTypeHash() const override;

	virtual uint32 GetMemoryFootprint(void) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual void CreateRenderThreadResources() override;

	virtual void DestroyRenderThreadResources() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const override;

	FMeshBatch* CreateMeshBatch(class FMeshElementCollector& Collector) const;
	
	virtual void DispatchComputePass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily) override;
};

/** Vis mesh scene proxy */
class FVisMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	virtual SIZE_T GetTypeHash() const override;

	FVisMeshSceneProxy(UVisMeshProceduralComponent* Component);

	virtual ~FVisMeshSceneProxy() override;

	void UpdateSection_RenderThread(FRHICommandListBase& RHICmdList, FVisMeshSectionUpdateData* SectionData);

	void SetSectionVisibility_RenderThread(int32 SectionIndex, bool bNewVisibility);

	// 收集每个view下每个LOD的FPrimitiveSceneProxy，并转换成FMeshBatch
	// 设置FMeshBatch中的FMeshBatchElement中的IndexBuffer, NumPrimitive, UniformBuffer等等关于渲染的东西
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,uint32 VisibilityMap, class FMeshElementCollector& Collector) const override;

	// 用于确定View渲染的相关性，可以认为是MeshPass的第一层过滤，用于确定是否参与某些特性的绘制
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual bool CanBeOccluded() const override;

	virtual uint32 GetMemoryFootprint(void) const override;

private:
	// Array of sections
	TArray<FVisMeshProxySection*> Sections;

	UBodySetup* BodySetup;

	FMaterialRelevance MaterialRelevance;
};
