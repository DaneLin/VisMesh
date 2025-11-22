#include "Utils/VisMeshUtils.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "RenderGraphUtils.h"
#include "RenderBase/VisMeshDispatchShaders.h"

DECLARE_GPU_DRAWCALL_STAT(PopulateVertexPass);

void AddPopulateVertexPass(FRDGBuilder& GraphBuilder, FRHIUnorderedAccessView* PositionsUAV,FRHIUnorderedAccessView* IndirectArgsBufferUAV, float InXSpace, float InYSpace, int32 InNumColumns,int32 InNumInstances, float InTime)
{
	RDG_GPU_STAT_SCOPE(GraphBuilder, PopulateVertexPass); // for unreal insights
	RDG_EVENT_SCOPE(GraphBuilder, "PopulateVertexPass"); // for render doc

	TShaderMapRef<FPopulateVertexAndIndirectBufferCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	FPopulateVertexAndIndirectBufferCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPopulateVertexAndIndirectBufferCS::FParameters>();
	PassParameters->OutInstanceVertices = PositionsUAV;
	PassParameters->OutIndirectArgs = IndirectArgsBufferUAV;
	PassParameters->XSpace = InXSpace;
	PassParameters->YSpace = InYSpace;
	PassParameters->NumColumns = InNumColumns;
	PassParameters->NumInstances = InNumInstances;
	PassParameters->Time = InTime;

	// 计算 GroupCount
	int32 GroupCount = FMath::DivideAndRoundUp(InNumInstances,(int32)FPopulateVertexAndIndirectBufferCS::ThreadGroupSize);

	FComputeShaderUtils::AddPass(GraphBuilder,RDG_EVENT_NAME("PopulateVertexPass"),ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,ComputeShader,PassParameters,FIntVector(GroupCount, 1, 1));
}

void ConvertProcMeshToDynMeshVertex(FDynamicMeshVertex& Vert, const FVisMeshVertex& ProcVert)
{
	Vert.Position = static_cast<FVector3f>(ProcVert.Position);
	Vert.Color = ProcVert.Color;
	Vert.TextureCoordinate[0] = FVector2f(ProcVert.UV0); // LWC_TODO: Precision loss
	Vert.TextureCoordinate[1] = FVector2f(ProcVert.UV1); // LWC_TODO: Precision loss
	Vert.TextureCoordinate[2] = FVector2f(ProcVert.UV2); // LWC_TODO: Precision loss
	Vert.TextureCoordinate[3] = FVector2f(ProcVert.UV3); // LWC_TODO: Precision loss
	Vert.TangentX = ProcVert.Tangent.TangentX;
	Vert.TangentZ = ProcVert.Normal;
	Vert.TangentZ.Vector.W = ProcVert.Tangent.bFlipTangentY ? -127 : 127;
}
