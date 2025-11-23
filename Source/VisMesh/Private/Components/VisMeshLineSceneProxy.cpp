#include "Components/VisMeshLineSceneProxy.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "SceneRendererInterface.h"
#include "Components/VisMeshLineComponent.h"
#include "Utils/VisMeshUtils.h"

FVisMeshLineSceneProxy::FVisMeshLineSceneProxy(UVisMeshLineComponent* Owner)
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
	LineWidth = Owner->LineWidth;
}

SIZE_T FVisMeshLineSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FVisMeshLineSceneProxy::GetMemoryFootprint() const
{
	return(sizeof(*this) + GetAllocatedSize());
}

FPrimitiveViewRelevance FVisMeshLineSceneProxy::GetViewRelevance(const FSceneView* View) const
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

void FVisMeshLineSceneProxy::CreateRenderThreadResources()
{
	check(VertexFactory == nullptr);

	FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

	int32 TotalVertices = NumInstances * GNumVertsPerBox * 2;

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

void FVisMeshLineSceneProxy::DestroyRenderThreadResources()
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

void FVisMeshLineSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views,const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (FMeshBatch* MeshBatch = CreateMeshBatch(Collector))
		{
			Collector.AddMesh(ViewIndex, *MeshBatch);
		}
	}
}

FMeshBatch* FVisMeshLineSceneProxy::CreateMeshBatch(class FMeshElementCollector& Collector) const
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

	MeshBatch.CastShadow = true;
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

void FVisMeshLineSceneProxy::DispatchComputePass_RenderThread(FRDGBuilder& GraphBuilder,
	const FSceneViewFamily& ViewFamily)
{
	if (PositionBuffer && IndirectArgsBuffer)
	{
		const float CurrentTime = ViewFamily.Time.GetRealTimeSeconds();
		FVector WorldCameraPos = FVector::Zero();
		if (ViewFamily.Views.Num() > 0)
		{
			// 获取第一个视图 (通常是主要玩家视图或编辑器视口)
			const FSceneView* MainView = ViewFamily.Views[0];
           
			// 获取视图的世界原点
			WorldCameraPos = MainView->ViewMatrices.GetViewOrigin();
		}
		
		// 将相机从 世界空间 转换到 局部空间
		// 这样 Shader 里 LocalCamera - LocalVertex 才是正确的方向
		FVector LocalCameraPos = GetLocalToWorld().InverseTransformPosition(WorldCameraPos);

		// 4. 转为 float3 传给 Shader
		FVector3f CameraPosFloat = (FVector3f)LocalCameraPos; // 使用转换后的 Local 位置
            
		// 调用具体的 Pass 添加函数 (这个函数可以是静态的，或者 VisMeshUtils 里的)
		AddBoxWireframePass(GraphBuilder,PositionBuffer->GetUAV(),IndirectArgsBufferUAV,XSpace, YSpace, NumColumns, NumInstances, LineWidth,CurrentTime, CameraPosFloat);
	}
}