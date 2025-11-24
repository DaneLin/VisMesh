#pragma once

#include "RenderBase/VisMeshRenderResources.h"

static constexpr int32 GNumVertsPerBox = 36;

///
//// Utils
///

// 辅助函数：生成单位立方体 (0,0,0 到 1,1,1) 的36个顶点
static void GetUnitCubeVertices(TArray<FVector3f>& OutVertices);

///
//// Passes
///
static void AddPopulateVertexPass(FRDGBuilder& GraphBuilder, FRHIUnorderedAccessView* PositionsUAV,
								  FRHIUnorderedAccessView* IndirectArgsBufferUAV, float InXSpace, float InYSpace,
								  int32 InNumColumns, int32 InNumInstances, float InTime);



static void AddBoxChartInstancingPass(FRDGBuilder& GraphBuilder, FRHIUnorderedAccessView* InstanceOriginBuffersUAV,
									  FRHIUnorderedAccessView* InstanceTransformsUAV,
									  FRHIUnorderedAccessView* IndirectArgsBufferUAV, float InXSpace, float InYSpace,
									  int32 InNumColumns, int32 InNumInstances, float InTime);

static void AddBoxChartFrustumCulledInstancePass(FRDGBuilder& GraphBuilder, FRHIUnorderedAccessView* InstanceOriginBuffersUAV,
									  FRHIUnorderedAccessView* InstanceTransformsUAV,
									  FRDGBufferUAVRef IndirectArgsBufferUAV, float InXSpace, float InYSpace,
									  int32 InNumColumns, int32 InNumInstances, float InTime, FMatrix44f InProjectionViewMatrix);


static void AddBoxWireframePass(FRDGBuilder& GraphBuilder, FRHIUnorderedAccessView* PositionsUAV,
								FRHIUnorderedAccessView* IndirectArgsBufferUAV, float InXSpace, float InYSpace,
								int32 InNumColumns, int32 InNumInstances, float InLineWidth, float InTime, FVector4f InCameraPosition);