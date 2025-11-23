#pragma once

#include "RenderBase/VisMeshRenderResources.h"

static constexpr int32 GNumVertsPerBox = 36;

static void AddPopulateVertexPass(FRDGBuilder& GraphBuilder, FRHIUnorderedAccessView* PositionsUAV,
                                  FRHIUnorderedAccessView* IndirectArgsBufferUAV, float InXSpace, float InYSpace,
                                  int32 InNumColumns, int32 InNumInstances, float InTime);

/** 
 *	Struct used to send update to mesh data 
 *	Arrays may be empty, in which case no update is performed.
 */
static void ConvertProcMeshToDynMeshVertex(FDynamicMeshVertex& Vert, const FVisMeshVertex& ProcVert);

static void AddBoxChartInstancingPass(FRDGBuilder& GraphBuilder,
                                      FRHIUnorderedAccessView* InstanceTransformsUAV,
                                      FRHIUnorderedAccessView* IndirectArgsBufferUAV,
                                      float InXSpace, float InYSpace,
                                      int32 InNumColumns, int32 InNumInstances, float InTime);

// 辅助函数：生成单位立方体 (0,0,0 到 1,1,1) 的36个顶点
// 这样配合 Scale = 50, Height = BarHeight 的变换矩阵，正好符合原本的逻辑
static void GetUnitCubeVertices(TArray<FVector3f>& OutVertices);
