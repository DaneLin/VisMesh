#include "Utils/VisMeshUtils.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "RenderGraphUtils.h"
#include "RenderBase/VisMeshDispatchShaders.h"

// --------------------------------------------------------
// ------------------------Utils---------------------------
// --------------------------------------------------------

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

void GetUnitCubeVertices(TArray<FVector3f>& OutVertices)
{
	OutVertices.Empty(36);

	// 定义 8 个角点 (0到1范围)
	FVector3f p0(0, 0, 0);
	FVector3f p1(1, 0, 0);
	FVector3f p2(1, 1, 0);
	FVector3f p3(0, 1, 0);
	FVector3f p4(0, 0, 1);
	FVector3f p5(1, 0, 1);
	FVector3f p6(1, 1, 1);
	FVector3f p7(0, 1, 1);

	// 定义6个面，每个面2个三角形，按逆时针(CCW)顺序
	// 1. Bottom (-Z)
	OutVertices.Add(p0); OutVertices.Add(p2); OutVertices.Add(p1);
	OutVertices.Add(p0); OutVertices.Add(p3); OutVertices.Add(p2);

	// 2. Top (+Z)
	OutVertices.Add(p4); OutVertices.Add(p5); OutVertices.Add(p6);
	OutVertices.Add(p4); OutVertices.Add(p6); OutVertices.Add(p7);

	// 3. Front (-Y)
	OutVertices.Add(p0); OutVertices.Add(p1); OutVertices.Add(p5);
	OutVertices.Add(p0); OutVertices.Add(p5); OutVertices.Add(p4);

	// 4. Back (+Y)
	OutVertices.Add(p3); OutVertices.Add(p6); OutVertices.Add(p2);
	OutVertices.Add(p3); OutVertices.Add(p7); OutVertices.Add(p6);

	// 5. Left (-X)
	OutVertices.Add(p0); OutVertices.Add(p4); OutVertices.Add(p7);
	OutVertices.Add(p0); OutVertices.Add(p7); OutVertices.Add(p3);

	// 6. Right (+X)
	OutVertices.Add(p1); OutVertices.Add(p2); OutVertices.Add(p6);
	OutVertices.Add(p1); OutVertices.Add(p6); OutVertices.Add(p5);
}

// --------------------------------------------------------
// ------------------------Passes--------------------------
// --------------------------------------------------------

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

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("PopulateVertexPass"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,PassParameters,
		FIntVector(GroupCount, 1, 1));
}

DECLARE_GPU_DRAWCALL_STAT(PopulateInstancePass);

void AddBoxChartInstancingPass(FRDGBuilder& GraphBuilder,FRHIUnorderedAccessView* InstanceOriginBuffersUAV, FRHIUnorderedAccessView* InstanceTransformsUAV,
	FRHIUnorderedAccessView* IndirectArgsBufferUAV, float InXSpace, float InYSpace, int32 InNumColumns,
	int32 InNumInstances, float InTime)
{
	RDG_GPU_STAT_SCOPE(GraphBuilder, PopulateInstancePass);
	RDG_EVENT_SCOPE(GraphBuilder, "PopulateInstancePass");

	TShaderMapRef<FPopulateBoxChartInstanceBufferCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	FPopulateBoxChartInstanceBufferCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPopulateBoxChartInstanceBufferCS::FParameters>();
    PassParameters->OutInstanceOriginBuffer = InstanceOriginBuffersUAV;
	PassParameters->OutInstanceTransforms = InstanceTransformsUAV;
	PassParameters->OutIndirectArgs = IndirectArgsBufferUAV;
	PassParameters->XSpace = InXSpace;
	PassParameters->YSpace = InYSpace;
	PassParameters->NumColumns = InNumColumns;
	PassParameters->NumInstances = InNumInstances;
	PassParameters->Time = InTime;

	int32 GroupCount = FMath::DivideAndRoundUp(InNumInstances, (int32)FPopulateVertexAndIndirectBufferCS::ThreadGroupSize);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("PopulateInstanceTransforms"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		PassParameters,
		FIntVector(GroupCount, 1, 1)
	);
}

DECLARE_GPU_DRAWCALL_STAT(PopulateWireFramePass);

void AddBoxWireframePass(FRDGBuilder& GraphBuilder, FRHIUnorderedAccessView* PositionsUAV,
	FRHIUnorderedAccessView* IndirectArgsBufferUAV, float InXSpace, float InYSpace, int32 InNumColumns,
	int32 InNumInstances, float InLineWidth, float InTime, FVector4f InCameraPosition)
{
	RDG_GPU_STAT_SCOPE(GraphBuilder, PopulateWireFramePass); // for unreal insights
	RDG_EVENT_SCOPE(GraphBuilder, "PopulateWireFramePass"); // for render doc

	TShaderMapRef<FPopulateBoxWireframeBufferCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	FPopulateBoxWireframeBufferCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPopulateBoxWireframeBufferCS::FParameters>();
	PassParameters->OutInstanceVertices = PositionsUAV;
	PassParameters->OutIndirectArgs = IndirectArgsBufferUAV;
	PassParameters->XSpace = InXSpace;
	PassParameters->YSpace = InYSpace;
	PassParameters->NumColumns = InNumColumns;
	PassParameters->NumInstances = InNumInstances;
	PassParameters->LineWidth = InLineWidth;
	PassParameters->Time = InTime;
	PassParameters->CameraPos = InCameraPosition;

	// 计算 GroupCount
	int32 GroupCount = FMath::DivideAndRoundUp(InNumInstances,(int32)FPopulateBoxWireframeBufferCS::ThreadGroupSize);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("PopulateWireFramePass"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		PassParameters,
		FIntVector(GroupCount, 1, 1));
}