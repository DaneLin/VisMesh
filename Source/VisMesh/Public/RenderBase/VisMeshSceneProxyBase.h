#pragma once

#include "PrimitiveSceneProxy.h"

class UVisMeshProceduralComponent;
class FVisMeshSectionUpdateData;
class FVisMeshProxySection;
class UVisMeshIndirectComponent;
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

class FVisMeshSceneProxyBase : public FPrimitiveSceneProxy
{
public:
	// 继承父类构造函数
	FVisMeshSceneProxyBase(const UPrimitiveComponent* InComponent, FName ResourceName = NAME_None)
		: FPrimitiveSceneProxy(InComponent, ResourceName)
	{}

	// 这是我们要调用的纯虚函数
	virtual void DispatchComputePass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily) = 0;
};

