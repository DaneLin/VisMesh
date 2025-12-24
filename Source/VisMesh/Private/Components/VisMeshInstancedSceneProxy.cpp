#include "Components/VisMeshInstancedSceneProxy.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "Components/VisMeshInstancedComponent.h"
#include "Experimental/Graph/GraphConvert.h"
#include "RenderBase/VisMeshInstancedVertexFactory.h"
#include "Utils/VisMeshUtils.h"

FVisMeshInstancedSceneProxy::FVisMeshInstancedSceneProxy(UVisMeshInstancedComponent* Owner)
	: FVisMeshSceneProxyBase(Owner)
	  , VertexFactory{nullptr}
	  , PositionBuffer{nullptr}
	  , IndirectArgsBuffer{nullptr}
	  , InstanceBuffer{nullptr}
		, IndexBuffer{nullptr}
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

void GetUnitCubeIndices(TArray<uint32>& OutIndices)
{
	OutIndices.Empty(36);

	// 顶点排列:
	//     7-------6
	//    /|      /|
	//   4-------5 |
	//   | 3-----|-2
	//   |/      |/
	//   0-------1

	// 1. Bottom (-Z): 0,1,2,3
	OutIndices.Add(0); OutIndices.Add(1); OutIndices.Add(2);
	OutIndices.Add(0); OutIndices.Add(2); OutIndices.Add(3);

	// 2. Top (+Z): 4,5,6,7
	OutIndices.Add(4); OutIndices.Add(6); OutIndices.Add(5);
	OutIndices.Add(4); OutIndices.Add(7); OutIndices.Add(6);

	// 3. Front (-Y): 0,1,5,4
	OutIndices.Add(0); OutIndices.Add(5); OutIndices.Add(1);
	OutIndices.Add(0); OutIndices.Add(4); OutIndices.Add(5);

	// 4. Back (+Y): 3,7,6,2
	OutIndices.Add(3); OutIndices.Add(2); OutIndices.Add(6);
	OutIndices.Add(3); OutIndices.Add(6); OutIndices.Add(7);

	// 5. Left (-X): 0,4,7,3
	OutIndices.Add(0); OutIndices.Add(7); OutIndices.Add(4);
	OutIndices.Add(0); OutIndices.Add(3); OutIndices.Add(7);

	// 6. Right (+X): 1,2,6,5
	OutIndices.Add(1); OutIndices.Add(6); OutIndices.Add(2);
	OutIndices.Add(1); OutIndices.Add(5); OutIndices.Add(6);
}

void FVisMeshInstancedSceneProxy::CreateRenderThreadResources()
{
	check(VertexFactory == nullptr);

	FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

	// ========================================================================
	// 1. 初始化 PositionBuffer (标准几何体)
	// ========================================================================
	{
		PositionBuffer = new FPositionUAVVertexBuffer(8);
		PositionBuffer->InitResource(RHICmdList);

		TArray<FVector3f> UnitCubeVerts;
		GetUnitCubeVertices(UnitCubeVerts);

		check(UnitCubeVerts.Num() == 8);

		// 锁定 Buffer 进行写入
		void* BufferData = RHICmdList.LockBuffer(
			PositionBuffer->VertexBufferRHI, 
			0, 
			8 * sizeof(FVector3f), 
			RLM_WriteOnly
		);
        
		// 拷贝数据
		FMemory::Memcpy(BufferData, UnitCubeVerts.GetData(), UnitCubeVerts.Num() * sizeof(FVector3f));
        
		// 解锁
		RHICmdList.UnlockBuffer(PositionBuffer->VertexBufferRHI);
	}

	{
		TArray<uint32> Indices;
		GetUnitCubeIndices(Indices);  // 获取36个索引
        
		IndexBuffer = new FVisMeshIndexBuffer(Indices);
		IndexBuffer->InitResource(RHICmdList);  // 关键：调用 InitResource
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
	
	// 必须包含 DrawIndirect

	const uint32 IndirectSize = sizeof(FRHIDrawIndexedIndirectParameters);

	// ========================================================================
    // 1. 定义描述符 (为了告诉 RDG 这是什么类型的资源)
    // ========================================================================
    const uint32 NumElements = sizeof(FRHIDrawIndexedIndirectParameters) / sizeof(uint32);
    const uint32 BytesPerElement = sizeof(uint32); // R32_UINT

    FRDGBufferDesc IndirectDesc = FRDGBufferDesc::CreateIndirectDesc(NumElements * BytesPerElement);
    // 必须加上 UnorderedAccess，否则 RDG 创建 UAV 会失败
    IndirectDesc.Usage |= EBufferUsageFlags::UnorderedAccess;

    // ========================================================================
    // 2. 手动创建底层的 RHI Buffer
    // ========================================================================
    FRHIResourceCreateInfo IndirectCreateInfo(TEXT("VisMeshIndirectArgs"));
    
    // 使用 CreateBuffer 而不是 CreateVertexBuffer (UE5 推荐)，或者保持原样
    // 注意：Usage 必须包含 DrawIndirect 和 UnorderedAccess
    EBufferUsageFlags IndirectUsage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::DrawIndirect | EBufferUsageFlags::ShaderResource;

    FBufferRHIRef RawIndirectBuffer = RHICmdList.CreateBuffer(
        IndirectSize,
        IndirectUsage,
        IndirectDesc.BytesPerElement,
        ERHIAccess::IndirectArgs, // 初始状态
        IndirectCreateInfo
    );

    // ========================================================================
    // 3. 将 RHI Buffer 包装成 FRDGPooledBuffer
    // ========================================================================
    // 使用你贴出的构造函数: FRDGPooledBuffer(RHICmdList, InBuffer, InDesc, NumAllocatedElements, Name)
    IndirectArgsBuffer = new FRDGPooledBuffer(
        RHICmdList,
        RawIndirectBuffer,
        IndirectDesc,
        IndirectDesc.NumElements,
        TEXT("VisMeshIndirectArgs")
    );

    // ========================================================================
    // 4. 创建 RHI UAV (供 MeshBatch 使用)
    // ========================================================================
    // 这一步和之前一样，MeshBatch 需要 RHI 层的 UAV
    if (RawIndirectBuffer.IsValid())
    {
        IndirectArgsBufferUAV = RHICmdList.CreateUnorderedAccessView(
            RawIndirectBuffer,
            FRHIViewDesc::CreateBufferUAV()
                .SetTypeFromBuffer(RawIndirectBuffer)
                .SetFormat(PF_R32_UINT)
                .SetNumElements(NumElements)
        );
    }
}

void FVisMeshInstancedSceneProxy::DestroyRenderThreadResources()
{
	if (VertexFactory != nullptr)
	{
		VertexFactory->ReleaseResource();
		delete VertexFactory;
		VertexFactory = nullptr;
	}
	if (IndexBuffer)
	{
		IndexBuffer->ReleaseResource();
		delete IndexBuffer;
		IndexBuffer = nullptr;
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
	if (!Material || !VertexFactory || !PositionBuffer || !IndirectArgsBuffer || !InstanceBuffer || !IndexBuffer)
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
	MeshBatch.Type = PT_TriangleList;

	MeshBatchElement.IndexBuffer = IndexBuffer;
	MeshBatchElement.FirstIndex = 0;
	MeshBatchElement.NumPrimitives = 12;  // 36 indices / 3 = 12 triangles
	MeshBatchElement.MinVertexIndex = 0;
	MeshBatchElement.MaxVertexIndex = 7;

	MeshBatchElement.NumInstances = 1;  // 需要设置一个非零值
	MeshBatchElement.BaseVertexIndex = 0;
	MeshBatchElement.UserIndex = 0;  // InstanceOffset

	MeshBatchElement.NumPrimitives = 0;
	MeshBatchElement.IndirectArgsBuffer = IndirectArgsBuffer->GetRHI();
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
		// 1. 注册外部资源 (前提：IndirectArgsBuffer 类型已改为 TRefCountPtr<FRDGPooledBuffer>)
		FRDGBufferRef IndirectArgsRDG = GraphBuilder.RegisterExternalBuffer(IndirectArgsBuffer);


		FRDGBufferUAVRef ClearUAV = GraphBuilder.CreateUAV(IndirectArgsRDG, PF_R32_UINT);
		
		//AddCopyBufferPass(GraphBuilder, IndirectArgsRDG, ResetBuffer);

		AddClearUAVPass(GraphBuilder, ClearUAV, 0);

		FRDGBufferUAVRef IndirectArgsUAVRDG = GraphBuilder.CreateUAV(IndirectArgsRDG, PF_R32_UINT);
		
		// 1. 获取主视图的 ViewProjectionMatrix
		FMatrix ViewProjMatrix = FMatrix::Identity;
		if (ViewFamily.Views.Num() > 0)
		{
			const FSceneView* MainView = ViewFamily.Views[0];
			ViewProjMatrix = MainView->ViewMatrices.GetViewProjectionMatrix();
		}
		FMatrix44f ViewProjectionMatrix = FMatrix44f(ViewProjMatrix.GetTransposed());
		FMatrix44f ModelMatrix = FMatrix44f(GetLocalToWorld());
		// 调用具体的 Pass 添加函数 (这个函数可以是静态的，或者 VisMeshUtils 里的)
		AddBoxChartFrustumCulledInstancePass(GraphBuilder,
									InstanceBuffer->GetOriginUAV(),
		                          InstanceBuffer->GetTransformUAV(), // 传入 Instance Buffer UAV
		                          IndirectArgsUAVRDG,
		                          XSpace, YSpace, NumColumns, NumInstances, CurrentTime,ViewProjectionMatrix, ModelMatrix
		);
	}
}
