#include "Components/VisMeshInstancedSceneProxy.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "Components/VisMeshInstancedComponent.h"
#include "RenderBase/VisMeshInstancedVertexFactory.h"
#include "Utils/VisMeshUtils.h"

FVisMeshInstancedSceneProxy::FVisMeshInstancedSceneProxy(UVisMeshInstancedComponent* Owner)
	: FVisMeshSceneProxyBase(Owner)
	  , VertexFactory{nullptr}
	  , PositionBuffer{nullptr}
	  , IndirectArgsBuffer{nullptr}
	  , InstanceBuffer{nullptr}
	  , Material(Owner->Material)
{
	bVFRequiresPrimitiveUniformBuffer = true;

	NumInstances = Owner->NumInstances > 1 ? Owner->NumInstances : 1;
	XSpace = Owner->XSpace;
	YSpace = Owner->YSpace;
	NumColumns = Owner->NumColumns;
}

SIZE_T FVisMeshInstancedSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FVisMeshInstancedSceneProxy::GetMemoryFootprint() const
{
	return (sizeof(*this) + GetAllocatedSize());
}

FPrimitiveViewRelevance FVisMeshInstancedSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = false;
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bRenderInDepthPass = ShouldRenderInDepthPass();
	Result.bVelocityRelevance = DrawsVelocity();
	return Result;
}

void FVisMeshInstancedSceneProxy::CreateRenderThreadResources()
{
	check(VertexFactory == nullptr);

	FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

	// ========================================================================
	// 1. 初始化 PositionBuffer (标准几何体)
	// ========================================================================
	{
		PositionBuffer = new FPositionUAVVertexBuffer(GNumVertsPerBox);
		PositionBuffer->InitResource(RHICmdList);

		TArray<FVector3f> UnitCubeVerts;
		GetUnitCubeVertices(UnitCubeVerts);

		check(UnitCubeVerts.Num() == GNumVertsPerBox);

		// 锁定 Buffer 进行写入
		void* BufferData = RHICmdList.LockBuffer(
			PositionBuffer->VertexBufferRHI, 
			0, 
			GNumVertsPerBox * sizeof(FVector3f), 
			RLM_WriteOnly
		);
        
		// 拷贝数据
		FMemory::Memcpy(BufferData, UnitCubeVerts.GetData(), UnitCubeVerts.Num() * sizeof(FVector3f));
        
		// 解锁
		RHICmdList.UnlockBuffer(PositionBuffer->VertexBufferRHI);
	}

	InstanceBuffer = new FVisMeshInstanceBuffer(NumInstances);
	InstanceBuffer->InitResource(RHICmdList);

	VertexFactory = new FVisMeshInstancedVertexFactory(GetScene().GetFeatureLevel(), "VisMeshInstancedVertexFactory");
	FLocalVertexFactory::FDataType NewData;
	NewData.PositionComponent = FVertexStreamComponent(PositionBuffer, 0, sizeof(FVector3f), VET_Float3);
	if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
	{
		NewData.PositionComponentSRV = PositionBuffer->GetSRV();
	}

	NewData.ColorComponent = FVertexStreamComponent(&GNullColorVertexBuffer, 0, 0, VET_Color,EVertexStreamUsage::ManualFetch);
	NewData.TangentBasisComponents[0] = FVertexStreamComponent(&GNullColorVertexBuffer, 0, 0, VET_PackedNormal,EVertexStreamUsage::ManualFetch);
	NewData.TangentBasisComponents[1] = FVertexStreamComponent(&GNullColorVertexBuffer, 0, 0, VET_PackedNormal,EVertexStreamUsage::ManualFetch);

	if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
	{
		NewData.ColorComponentsSRV = GNullColorVertexBuffer.VertexBufferSRV;
		NewData.TangentsSRV = GNullColorVertexBuffer.VertexBufferSRV;
		NewData.TextureCoordinatesSRV = GNullColorVertexBuffer.VertexBufferSRV;
	}

	FInstancedVisMeshDataType InstanceData;

	if (InstanceBuffer)
	{
		InstanceBuffer->BindToDataType(InstanceData);
	}

	VertexFactory->SetData(NewData, InstanceBuffer ? &InstanceData : nullptr);
	VertexFactory->InitResource(RHICmdList);

	

	FRHIResourceCreateInfo IndirectCreateInfo(TEXT("VisMeshIndirectArgs"));
	EBufferUsageFlags IndirectUsage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::DrawIndirect;
	// 必须包含 DrawIndirect

	const uint32 IndirectSize = sizeof(FRHIDrawIndirectParameters);

	IndirectArgsBuffer = RHICmdList.CreateVertexBuffer(IndirectSize, IndirectUsage, IndirectCreateInfo);

	IndirectArgsBufferUAV = RHICmdList.CreateUnorderedAccessView(IndirectArgsBuffer,
	                                                             FRHIViewDesc::CreateBufferUAV()
	                                                             .SetTypeFromBuffer(IndirectArgsBuffer)
	                                                             .SetFormat(PF_R32_UINT)
	                                                             .SetNumElements(uint32(
		                                                             sizeof(FRHIDrawIndirectParameters) / sizeof(
			                                                             uint32)))
	);
}

void FVisMeshInstancedSceneProxy::DestroyRenderThreadResources()
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
	if (InstanceBuffer)
	{
		InstanceBuffer->ReleaseResource();
		delete InstanceBuffer;
		InstanceBuffer = nullptr;
	}
}

void FVisMeshInstancedSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
                                                         const FSceneViewFamily& ViewFamily, uint32 VisibilityMap,
                                                         class FMeshElementCollector& Collector) const
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (FMeshBatch* MeshBatch = CreateMeshBatch(Collector))
		{
			Collector.AddMesh(ViewIndex, *MeshBatch);
		}
	}
}

FMeshBatch* FVisMeshInstancedSceneProxy::CreateMeshBatch(class FMeshElementCollector& Collector) const
{
	// 检查所有资源是否有效
	if (!Material || !VertexFactory || !PositionBuffer || !IndirectArgsBuffer || !InstanceBuffer)
	{
		return nullptr;
	}

	FMaterialRenderProxy* MaterialRenderProxy = Material->GetRenderProxy();
	if (MaterialRenderProxy == nullptr)
	{
		return nullptr;
	}

	FMeshBatch& MeshBatch = Collector.AllocateMesh();

	MeshBatch.CastShadow = true;
	MeshBatch.bUseForDepthPass = true;
	MeshBatch.SegmentIndex = 0;

	FMeshBatchElement& MeshBatchElement = MeshBatch.Elements[0];
	MeshBatch.VertexFactory = VertexFactory;
	MeshBatch.MaterialRenderProxy = MaterialRenderProxy;

	MeshBatchElement.NumPrimitives = 0;
	MeshBatchElement.IndirectArgsBuffer = IndirectArgsBuffer;
	MeshBatchElement.IndirectArgsOffset = 0;
	MeshBatchElement.PrimitiveUniformBuffer = GetUniformBuffer();

	check(MeshBatchElement.IndirectArgsBuffer);

	return &MeshBatch;
}

void FVisMeshInstancedSceneProxy::DispatchComputePass_RenderThread(FRDGBuilder& GraphBuilder,
                                                                   const FSceneViewFamily& ViewFamily)
{
	if (PositionBuffer && IndirectArgsBuffer && InstanceBuffer)
	{
		const float CurrentTime = ViewFamily.Time.GetRealTimeSeconds();

		// 调用具体的 Pass 添加函数 (这个函数可以是静态的，或者 VisMeshUtils 里的)
		AddBoxChartInstancingPass(GraphBuilder,
									InstanceBuffer->GetOriginUAV(),
		                          InstanceBuffer->GetTransformUAV(), // 传入 Instance Buffer UAV
		                          IndirectArgsBufferUAV,
		                          XSpace, YSpace, NumColumns, NumInstances, CurrentTime
		);
	}
}
