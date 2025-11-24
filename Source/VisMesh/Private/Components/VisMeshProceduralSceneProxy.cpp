#include "Components/VisMeshProceduralSceneProxy.h"

#include "MaterialDomain.h"
#include "Components/VisMeshProceduralComponent.h"
#include "Materials/MaterialRenderProxy.h"
#include "PhysicsEngine/BodySetup.h"
#include "Utils/VisMeshUtils.h"
#include "Async/ParallelFor.h"

//// FVisMeshProceduralSceneProxy

SIZE_T FVisMeshProceduralSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

FVisMeshProceduralSceneProxy::FVisMeshProceduralSceneProxy(UVisMeshProceduralComponent* Component)
	: FVisMeshSceneProxyBase(Component)
	  , BodySetup(Component->GetBodySetup())
	  , MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
{
	// Static copy each section
	const int32 NumSections = Component->VisMeshSections.Num();
	Sections.AddZeroed(NumSections);
	for (int SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
	{
		FVisMeshSection& SrcSection = Component->VisMeshSections[SectionIdx];
		// 检查 SOA 数据是否有效 (Triangles 和 Positions 是必须的)
		if (SrcSection.Data.Triangles.Num() > 0 && SrcSection.Data.Positions.Num() > 0)
		{
			FVisMeshProxySection* NewSection = new FVisMeshProxySection(GetScene().GetFeatureLevel());

			const int32 NumVerts = SrcSection.Data.NumVertices();
			
			// 1. 预分配 DynamicMeshVertex 数组
			TArray<FDynamicMeshVertex> Vertices;
			Vertices.SetNumUninitialized(NumVerts);

			// 2. 使用 ParallelFor 将 SOA 转为 AOS (FDynamicMeshVertex)
			// 这样可以充分利用多核加速初始数据的构建
			const FVisMeshData& Data = SrcSection.Data;

			ParallelFor(NumVerts, [&](int32 i)
			{
				FDynamicMeshVertex& Vert = Vertices[i];

				// Position
				Vert.Position = (FVector3f)Data.Positions[i];

				// Color (Optional)
				Vert.Color = Data.Colors.IsValidIndex(i) ? Data.Colors[i] : FColor::White;

				// Tangent Basis & Normal
				// 需要计算切线空间 (TangentX, TangentY, TangentZ)
				const FVector3f TangentX = Data.Tangents.IsValidIndex(i) ? (FVector3f)Data.Tangents[i].TangentX : FVector3f(1, 0, 0);
				const FVector3f TangentZ = Data.Normals.IsValidIndex(i) ? (FVector3f)Data.Normals[i] : FVector3f(0, 0, 1);
				const bool bFlipTangentY = Data.Tangents.IsValidIndex(i) ? Data.Tangents[i].bFlipTangentY : false;
				
				Vert.TangentX = TangentX;
				Vert.TangentZ = TangentZ;
				// Calculate Binormal (TangentY) using cross product and flip sign
				//Vert.TangentY = (TangentZ ^ TangentX) * (bFlipTangentY ? -1.f : 1.f);

				// UVs (0-3)
				Vert.TextureCoordinate[0] = Data.UV0.IsValidIndex(i) ? (FVector2f)Data.UV0[i] : FVector2f::ZeroVector;
				Vert.TextureCoordinate[1] = Data.UV1.IsValidIndex(i) ? (FVector2f)Data.UV1[i] : FVector2f::ZeroVector;
				Vert.TextureCoordinate[2] = Data.UV2.IsValidIndex(i) ? (FVector2f)Data.UV2[i] : FVector2f::ZeroVector;
				Vert.TextureCoordinate[3] = Data.UV3.IsValidIndex(i) ? (FVector2f)Data.UV3[i] : FVector2f::ZeroVector;
			});

			// Copy index buffer (int32 -> uint32 conversion via Memcpy)
			const int32 NumIndices = SrcSection.Data.Triangles.Num();
			NewSection->IndexBuffer.Indices.SetNumUninitialized(NumIndices);
			FMemory::Memcpy(NewSection->IndexBuffer.Indices.GetData(), SrcSection.Data.Triangles.GetData(), NumIndices * sizeof(uint32));

			// Init Vertex Buffers
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

FVisMeshProceduralSceneProxy::~FVisMeshProceduralSceneProxy()
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

void FVisMeshProceduralSceneProxy::UpdateSection_RenderThread(FRHICommandListBase& RHICmdList,FVisMeshSectionUpdateData* SectionData)
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
			
			// 获取 SOA 数据
			const FVisMeshData& NewData = SectionData->Data;
			const int32 NumVerts = NewData.NumVertices();

			// 确保顶点数匹配，否则无法仅更新 Buffer (需重建)
			if (NumVerts == Section->VertexBuffers.PositionVertexBuffer.GetNumVertices())
			{
				// --- 1. ParallelFor 更新 CPU 端数据容器 ---
				// FPositionVertexBuffer 等类的 VertexPosition() 和 SetVertex...() 实际上是操作 CPU 内存
				// 这里使用 ParallelFor 加速这些 CPU 内存的填充和数学计算 (如 PackedNormal 转换)
				
				ParallelFor(NumVerts, [&](int32 i)
				{
					// Update Position
					Section->VertexBuffers.PositionVertexBuffer.VertexPosition(i) = (FVector3f)NewData.Positions[i];

					// Update Color
					if (NewData.Colors.IsValidIndex(i))
					{
						Section->VertexBuffers.ColorVertexBuffer.VertexColor(i) = NewData.Colors[i];
					}

					// Update Tangents & UVs
					// 需要准备数据以填充 StaticMeshVertexBuffer
					FVector3f TangentX = NewData.Tangents.IsValidIndex(i) ? (FVector3f)NewData.Tangents[i].TangentX : FVector3f(1, 0, 0);
					FVector3f TangentZ = NewData.Normals.IsValidIndex(i) ? (FVector3f)NewData.Normals[i] : FVector3f(0, 0, 1);
					bool bFlip = NewData.Tangents.IsValidIndex(i) ? NewData.Tangents[i].bFlipTangentY : false;
					// 计算 BiNormal (TangentY)
					FVector3f TangentY = (TangentZ ^ TangentX) * (bFlip ? -1.f : 1.f);

					// SetVertexTangents 是线程安全的，只要索引 i 不冲突
					Section->VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(i, TangentX, TangentY, TangentZ);

					// Update UVs
					if (NewData.UV0.IsValidIndex(i)) Section->VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, 0, (FVector2f)NewData.UV0[i]);
					if (NewData.UV1.IsValidIndex(i)) Section->VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, 1, (FVector2f)NewData.UV1[i]);
					if (NewData.UV2.IsValidIndex(i)) Section->VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, 2, (FVector2f)NewData.UV2[i]);
					if (NewData.UV3.IsValidIndex(i)) Section->VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, 3, (FVector2f)NewData.UV3[i]);
				});

				// --- 2. 提交到 GPU (RHI Unlock & Memcpy) ---
				// 这些操作必须在 RenderThread 串行执行，但由于 CPU 数据已准备好，这里只是纯粹的 Memcpy

				{
					auto& VertexBuffer = Section->VertexBuffers.PositionVertexBuffer;
					void* VertexBufferData = RHICmdList.LockBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
					FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
					RHICmdList.UnlockBuffer(VertexBuffer.VertexBufferRHI);
				}

				{
					auto& VertexBuffer = Section->VertexBuffers.ColorVertexBuffer;
					void* VertexBufferData = RHICmdList.LockBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
					FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
					RHICmdList.UnlockBuffer(VertexBuffer.VertexBufferRHI);
				}

				{
					auto& VertexBuffer = Section->VertexBuffers.StaticMeshVertexBuffer;
					// Update Tangents Buffer
					void* TangentBufferData = RHICmdList.LockBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetTangentSize(), RLM_WriteOnly);
					FMemory::Memcpy(TangentBufferData, VertexBuffer.GetTangentData(), VertexBuffer.GetTangentSize());
					RHICmdList.UnlockBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI);
					
					// Update UV Buffer
					void* TexCoordBufferData = RHICmdList.LockBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetTexCoordSize(), RLM_WriteOnly);
					FMemory::Memcpy(TexCoordBufferData, VertexBuffer.GetTexCoordData(), VertexBuffer.GetTexCoordSize());
					RHICmdList.UnlockBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI);
				}
			}
		}

		// Free data sent from game thread
		// 注意：Data 中包含的 TArray 内存也会在此处析构
		delete SectionData;
	}
}

void FVisMeshProceduralSceneProxy::SetSectionVisibility_RenderThread(int32 SectionIndex, bool bNewVisibility)
{
	check(IsInRenderingThread());

	if (SectionIndex < Sections.Num() && Sections[SectionIndex] != nullptr)
	{
		Sections[SectionIndex]->bSectionVisible = bNewVisibility;
	}
}

void FVisMeshProceduralSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views,const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const
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

FPrimitiveViewRelevance FVisMeshProceduralSceneProxy::GetViewRelevance(const FSceneView* View) const
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

bool FVisMeshProceduralSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}

uint32 FVisMeshProceduralSceneProxy::GetMemoryFootprint() const
{
	return (sizeof(*this) + GetAllocatedSize());
}

void FVisMeshProceduralSceneProxy::DispatchComputePass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily)
{
}
