#include "Components/VisMeshIndirectSceneProxy.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "Components/VisMeshIndirectComponent.h"
#include "Utils/VisMeshUtils.h"

FVisMeshIndirectSceneProxy::FVisMeshIndirectSceneProxy(UVisMeshIndirectComponent* Owner)
	: FVisMeshSceneProxyBase(Owner)
	, VertexFactory{ nullptr }
	, PositionBuffer{ nullptr }
	, IndirectArgsBuffer{ nullptr }
	, IndirectArgsBufferUAV{}
	, Material(Owner->Material)
{
	bVFRequiresPrimitiveUniformBuffer = true;
	
	NumInstances = Owner->NumInstances > 1 ? Owner->NumInstances : 1;
	XSpace = Owner->XSpace;
	YSpace = Owner->YSpace;
	NumColumns = Owner->NumColumns;

	// Scatter
	BoundsMin = Owner->BoundsMin;
	BoundsMax = Owner->BoundsMax;
	Radius = Owner->Radius;
	NumPoints = Owner->NumPoints;
	bDrawSphere = Owner->bDrawSphere;
	bUpdateEverTick = Owner->bUpdateEverTick;

	bHasInitialUpdateRun = false;
}

SIZE_T FVisMeshIndirectSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FVisMeshIndirectSceneProxy::GetMemoryFootprint() const
{
	return(sizeof(*this) + GetAllocatedSize());
}

FPrimitiveViewRelevance FVisMeshIndirectSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bRenderInDepthPass = ShouldRenderInDepthPass();
	Result.bVelocityRelevance = DrawsVelocity();
	return Result;
}

void FVisMeshIndirectSceneProxy::CreateRenderThreadResources()
{
	check(VertexFactory == nullptr);
	bHasInitialUpdateRun = false;
	
	FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

	int32 TotalVertices = 0;
	if (!bDrawSphere)
		TotalVertices = NumInstances * GNumVertsPerBox;
	else
		TotalVertices = NumPoints * 8 * 6 * 6;

	PositionBuffer = new FPositionUAVVertexBuffer(TotalVertices);
	PositionBuffer->InitResource(RHICmdList);

	VertexFactory = new FLocalVertexFactory(GetScene().GetFeatureLevel(), "VisMeshIndirectVertexFactory");
	FLocalVertexFactory::FDataType NewData;
	NewData.PositionComponent = FVertexStreamComponent(PositionBuffer, 0, sizeof(FVector3f), VET_Float3);
	if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
	{
		NewData.PositionComponentSRV = PositionBuffer->GetSRV();
	}

	NewData.ColorComponent = FVertexStreamComponent(&GNullColorVertexBuffer, 0, 0, VET_Color, EVertexStreamUsage::ManualFetch);
	NewData.TangentBasisComponents[0] = FVertexStreamComponent(&GNullColorVertexBuffer, 0, 0, VET_PackedNormal, EVertexStreamUsage::ManualFetch);
	NewData.TangentBasisComponents[1] = FVertexStreamComponent(&GNullColorVertexBuffer, 0, 0, VET_PackedNormal, EVertexStreamUsage::ManualFetch);

	if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
	{
		NewData.ColorComponentsSRV = GNullColorVertexBuffer.VertexBufferSRV;
		NewData.TangentsSRV = GNullColorVertexBuffer.VertexBufferSRV;
		NewData.TextureCoordinatesSRV = GNullColorVertexBuffer.VertexBufferSRV;
	}

	VertexFactory->SetData(NewData);
	VertexFactory->InitResource(RHICmdList);

	FRHIResourceCreateInfo IndirectCreateInfo(TEXT("VisMeshIndirectArgs"));
	EBufferUsageFlags IndirectUsage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::DrawIndirect; // 必须包含 DrawIndirect

	// 注意：IndirectArgs 也是一种 VertexBuffer (在 RHI 层面通常用 CreateVertexBuffer 或 CreateStructuredBuffer)
	// 对于 Indirect 参数，通常将其视为 StructuredBuffer 或者简单的 ByteAddressBuffer
	const uint32 IndirectSize = sizeof(FRHIDrawIndirectParameters); 

	// 创建 Buffer
	IndirectArgsBuffer = RHICmdList.CreateVertexBuffer(IndirectSize, IndirectUsage, IndirectCreateInfo);
	
	IndirectArgsBufferUAV = RHICmdList.CreateUnorderedAccessView(IndirectArgsBuffer,
		FRHIViewDesc::CreateBufferUAV()
		.SetTypeFromBuffer(IndirectArgsBuffer)
		.SetFormat(PF_R32_UINT)
		.SetNumElements(uint32(sizeof(FRHIDrawIndirectParameters) / sizeof(uint32)))
	);
}

void FVisMeshIndirectSceneProxy::DestroyRenderThreadResources()
{
	
	if (VertexFactory != nullptr)
	{
		VertexFactory->ReleaseResource();
		delete VertexFactory;
		VertexFactory = nullptr;
	}
	if (PositionBuffer != nullptr)
	{
		PositionBuffer->ReleaseResource();
		delete PositionBuffer;
		PositionBuffer = nullptr;
	}
	if (IndirectArgsBuffer)
	{
		IndirectArgsBuffer.SafeRelease();
		IndirectArgsBuffer = nullptr;
	}
	
}

void FVisMeshIndirectSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views,const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (FMeshBatch* MeshBatch = CreateMeshBatch(Collector))
		{
			Collector.AddMesh(ViewIndex, *MeshBatch);
		}
	}
}

FMeshBatch* FVisMeshIndirectSceneProxy::CreateMeshBatch(class FMeshElementCollector& Collector) const
{
	if (Material==nullptr)
	{
		return nullptr;
	}
	
	FMaterialRenderProxy* MaterialRenderProxy = Material->GetRenderProxy();

	if (MaterialRenderProxy == nullptr)
	{
		return nullptr;
	}

	if (!VertexFactory || !PositionBuffer || !IndirectArgsBuffer)
	{
		return nullptr;
	}

	FMeshBatch& MeshBatch = Collector.AllocateMesh();

	MeshBatch.CastShadow = false;
	MeshBatch.bUseForDepthPass = true;
	MeshBatch.SegmentIndex = 0;

	FMeshBatchElement& MeshBatchElement = MeshBatch.Elements[0];
	MeshBatch.VertexFactory = VertexFactory;
	MeshBatch.MaterialRenderProxy = MaterialRenderProxy;

	MeshBatchElement.NumPrimitives = 0;
	MeshBatchElement.IndirectArgsBuffer = IndirectArgsBuffer;
	MeshBatchElement.IndirectArgsOffset = 0;

	check(MeshBatchElement.IndirectArgsBuffer);

	return &MeshBatch;
	
}

void FVisMeshIndirectSceneProxy::DispatchComputePass_RenderThread(FRDGBuilder& GraphBuilder,
	const FSceneViewFamily& ViewFamily)
{
	if (PositionBuffer && IndirectArgsBuffer)
	{
		// 逻辑：如果是每帧更新(bUpdateEverTick)，或者 尚未运行过初始化(!bHasInitialUpdateRun)，则执行
		bool bShouldDispatch = bUpdateEverTick || !bHasInitialUpdateRun;

		if (bShouldDispatch)
		{
			const float CurrentTime = ViewFamily.Time.GetRealTimeSeconds();

			if (!bDrawSphere)
			{
				// 绘制柱状图
				AddPopulateVertexPass(GraphBuilder, PositionBuffer->GetUAV(), IndirectArgsBufferUAV, XSpace, YSpace, NumColumns, NumInstances, CurrentTime);
			}
			else
			{
				// 绘制散点图
				// 注意：对于散点图，如果只运行一次，Time/Seed 参数最好是固定的，或者只在初始化时随机一次
				AddGenerateScatterPlotSpherePass(GraphBuilder, PositionBuffer->GetUAV(), IndirectArgsBufferUAV, BoundsMin, BoundsMax, Radius, NumPoints, CurrentTime,0, 1 );
			}

			// 标记已运行。
			// 如果 bUpdateEverTick 为 true，这行代码虽有多余但无害。
			// 如果 bUpdateEverTick 为 false，这行代码确保下一帧进来时 bShouldDispatch 为 false。
			bHasInitialUpdateRun = true;
		}
	}
}