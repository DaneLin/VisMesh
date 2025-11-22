#include "VisMeshSceneProxy.h"

#include <DataDrivenShaderPlatformInfo.h>

#include "MaterialDomain.h"
#include "VisMeshProceduralComponent.h"
#include "VisMeshRenderResources.h"
#include "VisMeshUtils.h"
#include "Examples/IndirectExampleActor.h"
#include "Materials/MaterialRenderProxy.h"
#include "PhysicsEngine/BodySetup.h"

FVisMeshIndirectSceneProxy::FVisMeshIndirectSceneProxy(UIndirectComponent* Owner)
	: FVisMeshBaseSceneProxy(Owner)
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

	FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

	int32 TotalVertices = NumInstances * GNumVertsPerBox;

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

void FVisMeshIndirectSceneProxy::DispatchComputePass_RenderThread(FRDGBuilder& GraphBuilder,
	const FSceneViewFamily& ViewFamily)
{
	if (PositionBuffer && IndirectArgsBuffer)
	{
		const float CurrentTime = ViewFamily.Time.GetRealTimeSeconds();
            
		// 调用具体的 Pass 添加函数 (这个函数可以是静态的，或者 VisMeshUtils 里的)
		AddPopulateVertexPass(GraphBuilder,PositionBuffer->GetUAV(),IndirectArgsBufferUAV,XSpace, YSpace, NumColumns, NumInstances, CurrentTime);
	}
}

//// FVisMeshSceneProxy

SIZE_T FVisMeshSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

FVisMeshSceneProxy::FVisMeshSceneProxy(UVisMeshProceduralComponent* Component)
	: FPrimitiveSceneProxy(Component)
	  , BodySetup(Component->GetBodySetup())
	  , MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
{
	// Static copy each section
	const int32 NumSections = Component->VisMeshSections.Num();
	Sections.AddZeroed(NumSections);
	for (int SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
	{
		FVisMeshSection& SrcSection = Component->VisMeshSections[SectionIdx];
		if (SrcSection.ProcIndexBuffer.Num() > 0 && SrcSection.ProcVertexBuffer.Num() > 0)
		{
			FVisMeshProxySection* NewSection = new FVisMeshProxySection(GetScene().GetFeatureLevel());

			// Copy data from vertex buffer
			const int32 NumVerts = SrcSection.ProcVertexBuffer.Num();

			// Allocate verts
			TArray<FDynamicMeshVertex> Vertices;
			Vertices.SetNumUninitialized(NumVerts);
			// Copy verts
			for (int VertIdx = 0; VertIdx < NumVerts; VertIdx++)
			{
				const FVisMeshVertex& ProcVert = SrcSection.ProcVertexBuffer[VertIdx];
				FDynamicMeshVertex& Vert = Vertices[VertIdx];
				ConvertProcMeshToDynMeshVertex(Vert, ProcVert);
			}

			// Copy index buffer
			NewSection->IndexBuffer.Indices = SrcSection.ProcIndexBuffer;

			NewSection->VertexBuffers.InitFromDynamicVertex(&NewSection->VertexFactory, Vertices, 4);

			// Enqueue initialization of render resource
			BeginInitResource(&NewSection->VertexBuffers.PositionVertexBuffer);
			BeginInitResource(&NewSection->VertexBuffers.StaticMeshVertexBuffer);
			BeginInitResource(&NewSection->VertexBuffers.ColorVertexBuffer);
			BeginInitResource(&NewSection->IndexBuffer);
			BeginInitResource(&NewSection->VertexFactory);

			// Grab material
			NewSection->Material = Component->GetMaterial(SectionIdx);
			if (NewSection->Material == nullptr)
			{
				NewSection->Material = UMaterial::GetDefaultMaterial(MD_Surface);
			}

			// Copy visibility info
			NewSection->bSectionVisible = SrcSection.bSectionVisible;

			// Save ref to new section
			Sections[SectionIdx] = NewSection;
		}
	}
}

FVisMeshSceneProxy::~FVisMeshSceneProxy()
{
	for (FVisMeshProxySection* Section : Sections)
	{
		if (Section != nullptr)
		{
			Section->VertexBuffers.PositionVertexBuffer.ReleaseResource();
			Section->VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
			Section->VertexBuffers.ColorVertexBuffer.ReleaseResource();
			Section->IndexBuffer.ReleaseResource();
			Section->VertexFactory.ReleaseResource();

			delete Section;
		}
	}
}

void FVisMeshSceneProxy::UpdateSection_RenderThread(FRHICommandListBase& RHICmdList,FVisMeshSectionUpdateData* SectionData)
{
	SCOPE_CYCLE_COUNTER(STAT_VisMesh_UpdateSectionRT);

	// Check if we have data
	if (SectionData != nullptr)
	{
		// Check it references a valid section
		if (SectionData->TargetSection < Sections.Num() &&
			Sections[SectionData->TargetSection] != nullptr)
		{
			FVisMeshProxySection* Section = Sections[SectionData->TargetSection];
			// Lock vertex buffer
			const int32 NumVerts = SectionData->NewVertexBuffer.Num();

			// Iterate through vertex data, copying in new info
			for (int32 i = 0; i < NumVerts; i++)
			{
				const FVisMeshVertex& ProcVert = SectionData->NewVertexBuffer[i];
				FDynamicMeshVertex Vertex;
				ConvertProcMeshToDynMeshVertex(Vertex, ProcVert);

				Section->VertexBuffers.PositionVertexBuffer.VertexPosition(i) = Vertex.Position;
				Section->VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(i, Vertex.TangentX.ToFVector3f(), Vertex.GetTangentY(), Vertex.TangentZ.ToFVector3f());
				Section->VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, 0, Vertex.TextureCoordinate[0]);
				Section->VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, 1, Vertex.TextureCoordinate[1]);
				Section->VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, 2, Vertex.TextureCoordinate[2]);
				Section->VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, 3, Vertex.TextureCoordinate[3]);
				Section->VertexBuffers.ColorVertexBuffer.VertexColor(i) = Vertex.Color;
			}

			{
				auto& VertexBuffer = Section->VertexBuffers.PositionVertexBuffer;
				void* VertexBufferData = RHICmdList.LockBuffer(VertexBuffer.VertexBufferRHI, 0,VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
				FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(),VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
				RHICmdList.UnlockBuffer(VertexBuffer.VertexBufferRHI);
			}

			{
				auto& VertexBuffer = Section->VertexBuffers.ColorVertexBuffer;
				void* VertexBufferData = RHICmdList.LockBuffer(VertexBuffer.VertexBufferRHI, 0,VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
				FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(),VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
				RHICmdList.UnlockBuffer(VertexBuffer.VertexBufferRHI);
			}

			{
				auto& VertexBuffer = Section->VertexBuffers.StaticMeshVertexBuffer;
				void* VertexBufferData = RHICmdList.LockBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI, 0,VertexBuffer.GetTangentSize(), RLM_WriteOnly);
				FMemory::Memcpy(VertexBufferData, VertexBuffer.GetTangentData(), VertexBuffer.GetTangentSize());
				RHICmdList.UnlockBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI);
			}

			{
				auto& VertexBuffer = Section->VertexBuffers.StaticMeshVertexBuffer;
				void* VertexBufferData = RHICmdList.LockBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI, 0,VertexBuffer.GetTexCoordSize(), RLM_WriteOnly);
				FMemory::Memcpy(VertexBufferData, VertexBuffer.GetTexCoordData(), VertexBuffer.GetTexCoordSize());
				RHICmdList.UnlockBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI);
			}
		}

		// Free data sent from game thread
		delete SectionData;
	}
}

void FVisMeshSceneProxy::SetSectionVisibility_RenderThread(int32 SectionIndex, bool bNewVisibility)
{
	check(IsInRenderingThread());

	if (SectionIndex < Sections.Num() && Sections[SectionIndex] != nullptr)
	{
		Sections[SectionIndex]->bSectionVisible = bNewVisibility;
	}
}

void FVisMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views,const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const
{
	// Set up wireframe material (if needed)
	const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

	FColoredMaterialRenderProxy* WireframeMaterialInstance = nullptr;
	if (bWireframe)
	{
		WireframeMaterialInstance = new FColoredMaterialRenderProxy(GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : NULL,FLinearColor(0, 0.5f, 1.f));
		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
	}

	for (const FVisMeshProxySection* Section : Sections)
	{
		if (Section != nullptr && Section->bSectionVisible)
		{
			FMaterialRenderProxy* MaterialProxy = bWireframe? WireframeMaterialInstance: Section->Material->GetRenderProxy();

			// For each view..
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					const FSceneView* View = Views[ViewIndex];
					// Draw the mesh
					FMeshBatch& Mesh = Collector.AllocateMesh();
					FMeshBatchElement& BatchElement = Mesh.Elements[0];
					BatchElement.IndexBuffer = &Section->IndexBuffer;
					Mesh.bWireframe = bWireframe;
					Mesh.VertexFactory = &Section->VertexFactory;
					Mesh.MaterialRenderProxy = MaterialProxy;

					bool bHasPrecomputedVolumetricLightmap;
					FMatrix PreviousLocalToWorld;
					int32 SingleCaptureIndex;
					bool bOutputVelocity;
					GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld,SingleCaptureIndex, bOutputVelocity);
					bOutputVelocity |= AlwaysHasVelocity();

					FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
					DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(),
					                                  GetLocalBounds(), GetLocalBounds(), ReceivesDecals(),
					                                  bHasPrecomputedVolumetricLightmap, bOutputVelocity,
					                                  GetCustomPrimitiveData());
					BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

					BatchElement.FirstIndex = 0;
					BatchElement.NumPrimitives = Section->IndexBuffer.Indices.Num() / 3;
					BatchElement.MinVertexIndex = 0;
					BatchElement.MaxVertexIndex = Section->VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
					Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = SDPG_World;
					Mesh.bCanApplyViewModeOverrides = false;
					Collector.AddMesh(ViewIndex, Mesh);
				}
			}
		}
	}

	// Draw bounds
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			// Draw simple collision as wireframe if 'show collision', and collision is enabled, and we are not using the complex as the simple
			if (ViewFamily.EngineShowFlags.Collision && IsCollisionEnabled() && BodySetup->GetCollisionTraceFlag()
				!= ECollisionTraceFlag::CTF_UseComplexAsSimple)
			{
				FTransform GeomTransform(GetLocalToWorld());
				BodySetup->AggGeom.GetAggGeom(GeomTransform,
				                              GetSelectionColor(FColor(157, 149, 223, 255), IsSelected(),
				                                                IsHovered()).ToFColor(true), NULL, false, false,
				                              AlwaysHasVelocity(), ViewIndex, Collector);
			}

			// Render bounds
			RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
		}
	}
#endif
}

FPrimitiveViewRelevance FVisMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
	return Result;
}

bool FVisMeshSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}

uint32 FVisMeshSceneProxy::GetMemoryFootprint() const
{
	return (sizeof(*this) + GetAllocatedSize());
}