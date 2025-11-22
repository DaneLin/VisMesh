#pragma once

#include "RenderBase/VisMeshRenderResources.h"

static constexpr int32 GNumVertsPerBox = 36;

static void AddPopulateVertexPass(FRDGBuilder& GraphBuilder, FRHIUnorderedAccessView* PositionsUAV, FRHIUnorderedAccessView* IndirectArgsBufferUAV, float InXSpace, float InYSpace, int32 InNumColumns, int32 InNumInstances, float InTime);

/** 
 *	Struct used to send update to mesh data 
 *	Arrays may be empty, in which case no update is performed.
 */
static void ConvertProcMeshToDynMeshVertex(FDynamicMeshVertex& Vert, const FVisMeshVertex& ProcVert);
