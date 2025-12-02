// Copyright ZJU CAD. All Rights Reserved.

#include "Components/VisMeshProceduralComponent.h"

#include "Components/VisMeshProceduralSceneProxy.h"
#include "RenderBase/VisMeshRenderResources.h"
#include "RenderBase/VisMeshSceneProxyBase.h"
#include "PhysicsEngine/BodySetup.h"
#include "Async/ParallelFor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VisMeshProceduralComponent)

UVisMeshProceduralComponent::UVisMeshProceduralComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseComplexAsSimpleCollision = true;
}

void UVisMeshProceduralComponent::CreateMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices,const TArray<int32>& Triangles, const TArray<FVector>& Normals,const TArray<FVector2D>& UV0,const TArray<FVector2D>& UV1, const TArray<FVector2D>& UV2,const TArray<FVector2D>& UV3,const TArray<FColor>& VertexColors, const TArray<FVisMeshTangent>& Tangents,bool bCreateCollision)
{
	SCOPE_CYCLE_COUNTER(STAT_VisMesh_CreateMeshSection);

	// 构造 SOA 数据
	FVisMeshData NewData;
	const int32 NumVerts = Vertices.Num();
	
	// 1. 直接内存拷贝 (Move Semantics 无法用于 const引用，但 TArray Copy 也是极快的 Memcpy)
	NewData.Positions = Vertices;
	NewData.Triangles = Triangles;
	
	// 2. 使用 ParallelFor 处理可能的缺失数据或默认值填充
	// Normals
	if (Normals.Num() == NumVerts) NewData.Normals = Normals;
	else {
		NewData.Normals.SetNumUninitialized(NumVerts);
		ParallelFor(NumVerts, [&](int32 i) { NewData.Normals[i] = FVector(0, 0, 1); });
	}

	// UV0
	if (UV0.Num() == NumVerts) NewData.UV0 = UV0;
	else NewData.UV0.SetNumZeroed(NumVerts);

	// UV1
	if (UV1.Num() == NumVerts) NewData.UV1 = UV1;
	else NewData.UV1.SetNumZeroed(NumVerts);

	// UV2
	if (UV2.Num() == NumVerts) NewData.UV2 = UV2;
	else NewData.UV2.SetNumZeroed(NumVerts);

	// UV3
	if (UV3.Num() == NumVerts) NewData.UV3 = UV3;
	else NewData.UV3.SetNumZeroed(NumVerts);

	if (VertexColors.Num() == NumVerts) NewData.Colors = VertexColors;
	else 
	{
		NewData.Colors.SetNumUninitialized(NumVerts);
		// 并行填充默认白色
		ParallelFor(NumVerts, [&](int32 i) { NewData.Colors[i] = FColor::White; });
	}

	if (Tangents.Num() == NumVerts) NewData.Tangents = Tangents;
	else NewData.Tangents.SetNumZeroed(NumVerts);
	
	// 调用高效的 Move 版本
	CreateMeshSection(SectionIndex, MoveTemp(NewData), bCreateCollision);
}

void UVisMeshProceduralComponent::CreateMeshSection(int32 SectionIndex, const FVisMeshData& MeshData,
	bool bCreateCollision)
{
	FVisMeshData TempData = MeshData;
	CreateMeshSection(SectionIndex, MoveTemp(TempData), bCreateCollision);
}

void UVisMeshProceduralComponent::CreateMeshSection(int32 SectionIndex, FVisMeshData&& MeshData, bool bCreateCollision)
{
	SCOPE_CYCLE_COUNTER(STAT_VisMesh_CreateMeshSection);

	if (SectionIndex >= VisMeshSections.Num())
	{
		VisMeshSections.SetNum(SectionIndex + 1, false);
	}
	FVisMeshSection& NewSection = VisMeshSections[SectionIndex];

	// 核心优化：Move 接管内存
	// MeshData 的内部指针直接转移给 NewSection.Data，原 MeshData 变空
	NewSection.Data = MoveTemp(MeshData);

	// 2. 数据补齐 (保持 SOA 长度一致)
	const int32 NumVerts = NewSection.Data.NumVertices();
	auto& D = NewSection.Data; // 简写

	if (D.Normals.Num() != NumVerts) D.Normals.Init(FVector(0, 0, 1), NumVerts);
	if (D.Colors.Num() != NumVerts) D.Colors.Init(FColor::White, NumVerts);
	if (D.Tangents.Num() != NumVerts) D.Tangents.Init(FVisMeshTangent(), NumVerts);
	if (D.UV0.Num() != NumVerts) D.UV0.Init(FVector2D::ZeroVector, NumVerts);
	if (D.UV1.Num() != NumVerts) D.UV1.Init(FVector2D::ZeroVector, NumVerts);
	if (D.UV2.Num() != NumVerts) D.UV2.Init(FVector2D::ZeroVector, NumVerts);
	if (D.UV3.Num() != NumVerts) D.UV3.Init(FVector2D::ZeroVector, NumVerts);

	// 3. 计算 Bounds (直接读 Data.Positions)
	NewSection.SectionLocalBox = FBox(D.Positions);
	NewSection.bEnableCollision = bCreateCollision;

	// 4. 触发后续更新
	UpdateLocalBounds();
	UpdateCollision();
	MarkRenderStateDirty();
}

void UVisMeshProceduralComponent::UpdateMeshSection(int32 SectionIndex, const FVisMeshData& MeshData)
{
	FVisMeshData TempData = MeshData; 
	UpdateMeshSection(SectionIndex, MoveTemp(TempData));
}

void UVisMeshProceduralComponent::UpdateMeshSection(int32 SectionIndex, FVisMeshData&& MeshData)
{
	SCOPE_CYCLE_COUNTER(STAT_VisMesh_UpdateSectionGT);

	if (!VisMeshSections.IsValidIndex(SectionIndex)) return;

	FVisMeshSection& Section = VisMeshSections[SectionIndex];
	const int32 NumVerts = Section.Data.NumVertices();

	// 安全检查
	if (NumVerts == 0) return;

	// --- 1. 更新内部数据 (Selective Move) ---
	// 如果 InputData 提供了某个数组，且长度匹配，则 Move 过来替换旧的
	// 这样用户只需填充 FVisMeshData 中需要更新的字段 (比如只填 Positions)
	bool bPositionsChanged = false;

	if (MeshData.Positions.Num() == NumVerts)
	{
		Section.Data.Positions = MoveTemp(MeshData.Positions);
		bPositionsChanged = true;
	}
	if (MeshData.Normals.Num() == NumVerts)   Section.Data.Normals   = MoveTemp(MeshData.Normals);
	if (MeshData.Colors.Num() == NumVerts)    Section.Data.Colors    = MoveTemp(MeshData.Colors);
	if (MeshData.Tangents.Num() == NumVerts)  Section.Data.Tangents  = MoveTemp(MeshData.Tangents);
	if (MeshData.UV0.Num() == NumVerts)       Section.Data.UV0       = MoveTemp(MeshData.UV0);
	if (MeshData.UV1.Num() == NumVerts)       Section.Data.UV1       = MoveTemp(MeshData.UV1);
	if (MeshData.UV2.Num() == NumVerts)       Section.Data.UV2       = MoveTemp(MeshData.UV2);
	if (MeshData.UV3.Num() == NumVerts)       Section.Data.UV3       = MoveTemp(MeshData.UV3);

	if (bPositionsChanged)
	{
		Section.SectionLocalBox = FBox(Section.Data.Positions);
		UpdateLocalBounds();
		if (Section.bEnableCollision)
		{
			BodyInstance.UpdateTriMeshVertices(Section.Data.Positions);
		}
	}

	// --- 2. 物理更新 (直接引用) ---
	if (Section.bEnableCollision)
	{
		// 直接传递 Section.Data.Positions
		if (VisMeshSections.Num() == 1)
		{
			BodyInstance.UpdateTriMeshVertices(Section.Data.Positions);
		}
		else
		{
			// 多 Section 情况的 Append 逻辑...
			TArray<FVector> AllPos;
			for(auto& S : VisMeshSections) if(S.bEnableCollision) AllPos.Append(S.Data.Positions);
			BodyInstance.UpdateTriMeshVertices(AllPos);
		}
	}

	// --- 3. 生成 RenderData (ParallelFor) ---
	if (SceneProxy && !IsRenderStateDirty())
	{
		FVisMeshSectionUpdateData* SectionData = new FVisMeshSectionUpdateData;
		SectionData->TargetSection = SectionIndex;
		SectionData->Data = Section.Data; // Copy

		FVisMeshProceduralSceneProxy* ProcMeshSceneProxy = (FVisMeshProceduralSceneProxy*)SceneProxy;
		ENQUEUE_RENDER_COMMAND(FVisMeshSectionUpdate)
		([ProcMeshSceneProxy, SectionData](FRHICommandListImmediate& RHICmdList)
		{
			ProcMeshSceneProxy->UpdateSection_RenderThread(RHICmdList, SectionData);
		});
	}
	MarkRenderTransformDirty();
}

void UVisMeshProceduralComponent::CreateMeshSection_LinearColor(int32 SectionIndex, const TArray<FVector>& Vertices,const TArray<int32>& Triangles, const TArray<FVector>& Normals,const TArray<FVector2D>& UV0,const TArray<FVector2D>& UV1, const TArray<FVector2D>& UV2,const TArray<FVector2D>& UV3,const TArray<FLinearColor>& VertexColors,const TArray<FVisMeshTangent>& Tangents, bool bCreateCollision,bool bSRGBConversion)
{
	// Convert FLinearColors to FColors
	TArray<FColor> Colors;
	if (VertexColors.Num() > 0)
	{
		Colors.SetNumUninitialized(VertexColors.Num());
		// ParallelFor 加速颜色转换
		ParallelFor(VertexColors.Num(), [&](int32 i)
		{
			Colors[i] = VertexColors[i].ToFColor(bSRGBConversion);
		});
	}

	CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UV0, UV1, UV2, UV3, Colors, Tangents, bCreateCollision);
}

void UVisMeshProceduralComponent::UpdateMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices,const TArray<FVector>& Normals, const TArray<FVector2D>& UV0,const TArray<FVector2D>& UV1,const TArray<FVector2D>& UV2, const TArray<FVector2D>& UV3,const TArray<FColor>& VertexColors,const TArray<FVisMeshTangent>& Tangents)
{
	SCOPE_CYCLE_COUNTER(STAT_VisMesh_UpdateSectionGT);

	if (SectionIndex >= VisMeshSections.Num()) return;

	FVisMeshSection& Section = VisMeshSections[SectionIndex];
	const int32 NumVerts = Section.Data.NumVertices();

	// 安全检查
	if (NumVerts == 0) return;
	if (Vertices.Num() > 0 && Vertices.Num() != NumVerts)
	{
		UE_LOG(LogVisComponent, Error, TEXT("UpdateMeshSection vertex count mismatch."));
		return;
	}

	bool bPositionsChanged = false;

	if (Vertices.Num() == NumVerts)
	{
		Section.Data.Positions = Vertices;
		bPositionsChanged = true;
	}
	if (Normals.Num() == NumVerts) Section.Data.Normals = Normals;
	if (VertexColors.Num() == NumVerts) Section.Data.Colors = VertexColors;
	if (Tangents.Num() == NumVerts) Section.Data.Tangents = Tangents;
	if (UV0.Num() == NumVerts) Section.Data.UV0 = UV0;
	if (UV1.Num() == NumVerts) Section.Data.UV1 = UV1;
	if (UV2.Num() == NumVerts) Section.Data.UV2 = UV2;
	if (UV3.Num() == NumVerts) Section.Data.UV3 = UV3;

	// 如果位置改变，需要更新 Bounds 和 Physics
	if (bPositionsChanged)
	{
		Section.SectionLocalBox = FBox(Section.Data.Positions);
		UpdateLocalBounds();

		if (Section.bEnableCollision)
		{
			// 直接传递 Positions 数组，无需像 AOS 那样提取
			if (VisMeshSections.Num() == 1)
			{
				BodyInstance.UpdateTriMeshVertices(Section.Data.Positions);
			}
			else
			{
				// 多 Section 需要合并
				TArray<FVector> AllPos;
				for (auto& S : VisMeshSections)
				{
					if (S.bEnableCollision) AllPos.Append(S.Data.Positions);
				}
				BodyInstance.UpdateTriMeshVertices(AllPos);
			}
		}
	}

	if (SceneProxy && !IsRenderStateDirty())
	{
		// 构造 SOA 更新包
		FVisMeshSectionUpdateData* SectionData = new FVisMeshSectionUpdateData;
		SectionData->TargetSection = SectionIndex;
		
		// 深拷贝需要更新的数据到 RenderPayload
		// 这里为了简化逻辑，拷贝整个 SOA 结构 (实际 TArray Copy-on-write 机制在跨线程时会发生深拷贝)
		// 优化点：可以只拷贝修改过的数组，但在 RenderThread 处理会变复杂
		SectionData->Data = Section.Data; 

		FVisMeshProceduralSceneProxy* ProcMeshSceneProxy = (FVisMeshProceduralSceneProxy*)SceneProxy;
		ENQUEUE_RENDER_COMMAND(FVisMeshSectionUpdate)
		([ProcMeshSceneProxy, SectionData](FRHICommandListImmediate& RHICmdList)
		{
			ProcMeshSceneProxy->UpdateSection_RenderThread(RHICmdList, SectionData);
		});
	}

	MarkRenderTransformDirty();
}

void UVisMeshProceduralComponent::UpdateMeshSection_LinearColor(int32 SectionIndex, const TArray<FVector>& Vertices,const TArray<FVector>& Normals, const TArray<FVector2D>& UV0,const TArray<FVector2D>& UV1,const TArray<FVector2D>& UV2, const TArray<FVector2D>& UV3,const TArray<FLinearColor>& VertexColors,const TArray<FVisMeshTangent>& Tangents, bool bSRGBConversion)
{
	// Convert FLinearColors to FColors
	TArray<FColor> Colors;
	if (VertexColors.Num() > 0)
	{
		Colors.SetNumUninitialized(VertexColors.Num());
		// ParallelFor 优化颜色转换
		ParallelFor(VertexColors.Num(), [&](int32 i)
		{
			Colors[i] = VertexColors[i].ToFColor(bSRGBConversion);
		});
	}

	UpdateMeshSection(SectionIndex, Vertices, Normals, UV0, UV1, UV2, UV3, Colors, Tangents);
}

// Called when the game starts
void UVisMeshProceduralComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
}


// Called every frame
void UVisMeshProceduralComponent::TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UVisMeshProceduralComponent::ClearMeshSection(int32 SectionIndex)
{
	if (SectionIndex < VisMeshSections.Num())
	{
		VisMeshSections[SectionIndex].Reset();
		UpdateLocalBounds();
		UpdateCollision();
		MarkRenderStateDirty();
	}
}

void UVisMeshProceduralComponent::ClearAllMeshSections()
{
	VisMeshSections.Empty();
	UpdateLocalBounds();
	UpdateCollision();
	MarkRenderStateDirty();
}

void UVisMeshProceduralComponent::SetMeshSectionVisible(int32 SectionIndex, bool bNewVisibility)
{
	if (SectionIndex < VisMeshSections.Num())
	{
		// Set game thread state
		VisMeshSections[SectionIndex].bSectionVisible = bNewVisibility;

		if (SceneProxy)
		{
			// Enqueue command to modify render thread info
			FVisMeshProceduralSceneProxy* ProcMeshSceneProxy = (FVisMeshProceduralSceneProxy*)SceneProxy;
			ENQUEUE_RENDER_COMMAND(FProcMeshSectionVisibilityUpdate)(
				[ProcMeshSceneProxy, SectionIndex, bNewVisibility](FRHICommandListImmediate& RHICmdList)
				{
					ProcMeshSceneProxy->SetSectionVisibility_RenderThread(SectionIndex, bNewVisibility);
				});
		}
	}
}

bool UVisMeshProceduralComponent::IsMeshSectionVisible(int32 SectionIndex) const
{
	return (SectionIndex < VisMeshSections.Num()) ? VisMeshSections[SectionIndex].bSectionVisible : false;
}

int32 UVisMeshProceduralComponent::GetNumSections() const
{
	return VisMeshSections.Num();
}

void UVisMeshProceduralComponent::AddCollisionConvexMesh(TArray<FVector> ConvexVerts)
{
	if (ConvexVerts.Num() >= 4)
	{
		// New element
		FKConvexElem NewConvexElem;
		// Copy in vertex info
		NewConvexElem.VertexData = ConvexVerts;
		// Update bounding box
		NewConvexElem.ElemBox = FBox(ConvexVerts);
		// Add to array of convex elements
		CollisionConvexElems.Add(NewConvexElem);
		// Refresh collision
		UpdateCollision();
	}
}

void UVisMeshProceduralComponent::ClearCollisionConvexMeshes()
{
	// Empty simple collision info
	CollisionConvexElems.Empty();
	// Refresh collision
	UpdateCollision();
}

void UVisMeshProceduralComponent::SetCollisionConvexMeshes(const TArray<TArray<FVector>>& ConvexMeshes)
{
	CollisionConvexElems.Reset();

	// Create element for each convex mesh
	for (int32 ConvexIndex = 0; ConvexIndex < ConvexMeshes.Num(); ConvexIndex++)
	{
		FKConvexElem NewConvexElem;
		NewConvexElem.VertexData = ConvexMeshes[ConvexIndex];
		NewConvexElem.ElemBox = FBox(NewConvexElem.VertexData);

		CollisionConvexElems.Add(NewConvexElem);
	}

	UpdateCollision();
}

bool UVisMeshProceduralComponent::GetTriMeshSizeEstimates(struct FTriMeshCollisionDataEstimates& OutTriMeshEstimates,bool bInUseAllTriData) const
{
	return IInterface_CollisionDataProvider::GetTriMeshSizeEstimates(OutTriMeshEstimates, bInUseAllTriData);
}

bool UVisMeshProceduralComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
	int32 VertexBase = 0; // 用于记录顶点偏移量

	// 1. 遍历所有 Section
	for (int32 SectionIdx = 0; SectionIdx < VisMeshSections.Num(); SectionIdx++)
	{
		const FVisMeshSection& Section = VisMeshSections[SectionIdx];
		// 检查是否有数据 且 (强制全部使用 OR 开启了碰撞)
		if (Section.Data.Triangles.Num() >= 3 && (InUseAllTriData || Section.bEnableCollision))
		{
			// 2. 拷贝顶点数据
			// Append 将新顶点添加到物理网格的顶点数组末尾
			CollisionData->Vertices.Append(Section.Data.Positions);

			// 3. 拷贝三角形索引
			// 注意：FVisMeshData 使用 TArray<int32>，物理数据使用 FTriIndices (struct {v0,v1,v2})
			// 并且索引需要加上当前的 VertexBase 偏移量
			const int32 NumTriangles = Section.Data.Triangles.Num() / 3;
			for (int32 i = 0; i < NumTriangles; i++)
			{
				FTriIndices Triangle;
				Triangle.v0 = Section.Data.Triangles[i * 3 + 0] + VertexBase;
				Triangle.v1 = Section.Data.Triangles[i * 3 + 1] + VertexBase;
				Triangle.v2 = Section.Data.Triangles[i * 3 + 2] + VertexBase;

				CollisionData->Indices.Add(Triangle);

				// 设置材质索引 (用于物理材质区分)
				CollisionData->MaterialIndices.Add(SectionIdx);
			}

			// 更新顶点偏移量，供下一个 Section 使用
			VertexBase = CollisionData->Vertices.Num();
		}
	}

	bool bResult = (CollisionData->Vertices.Num() > 0 && CollisionData->Indices.Num() > 0);
	return bResult;
}

bool UVisMeshProceduralComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	for (const FVisMeshSection& Section : VisMeshSections)
	{
		if (Section.Data.Triangles.Num() >= 3 && (InUseAllTriData || Section.bEnableCollision))
		{
			return true;
		}
	}
	return false;
}

FVisMeshSection* UVisMeshProceduralComponent::GetVisMeshSection(int32 SectionIndex)
{
	if (SectionIndex < VisMeshSections.Num())
	{
		return &VisMeshSections[SectionIndex];
	}
	else
	{
		return nullptr;
	}
}

void UVisMeshProceduralComponent::SetVisMeshSection(int32 SectionIndex, const FVisMeshSection& Section)
{
	// Ensure sections array is long enough
	if (SectionIndex >= VisMeshSections.Num())
	{
		VisMeshSections.SetNum(SectionIndex + 1, false);
	}

	VisMeshSections[SectionIndex] = Section;

	UpdateLocalBounds(); // Update overall bounds
	UpdateCollision(); // Mark collision as dirty
	MarkRenderStateDirty(); // New section requires recreating scene proxy
}

FPrimitiveSceneProxy* UVisMeshProceduralComponent::CreateSceneProxy()
{
	SCOPE_CYCLE_COUNTER(STAT_VisMesh_CreateSceneProxy);
	
	return new FVisMeshProceduralSceneProxy(this);
}

class UBodySetup* UVisMeshProceduralComponent::GetBodySetup()
{
	CreateVisMeshBodySetup();
	return VisMeshBodySetup;
}

UMaterialInterface* UVisMeshProceduralComponent::GetMaterialFromCollisionFaceIndex(int32 FaceIndex, int32& SectionIndex) const
{
	UMaterialInterface* Result = nullptr;
	SectionIndex = 0;

	if (FaceIndex >= 0)
	{
		int32 TotalFaceCount = 0;
		for (int32 SectionIdx = 0; SectionIdx < VisMeshSections.Num(); SectionIdx++)
		{
			const FVisMeshSection& Section = VisMeshSections[SectionIdx];
			int32 NumFaces = Section.Data.Triangles.Num() / 3; // SOA change
			TotalFaceCount += NumFaces;

			if (FaceIndex < TotalFaceCount)
			{
				Result = GetMaterial(SectionIdx);
				SectionIndex = SectionIdx;
				break;
			}
		}
	}
	return Result;
}

int32 UVisMeshProceduralComponent::GetNumMaterials() const
{
	return VisMeshSections.Num();
}

void UVisMeshProceduralComponent::PostLoad()
{
	Super::PostLoad();

	if (VisMeshBodySetup && IsTemplate())
	{
		VisMeshBodySetup->SetFlags(RF_Public | RF_ArchetypeObject);
	}
}

FBoxSphereBounds UVisMeshProceduralComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds Ret(LocalBounds.TransformBy(LocalToWorld));

	Ret.BoxExtent *= BoundsScale;
	Ret.SphereRadius *= BoundsScale;

	return Ret;
}

void UVisMeshProceduralComponent::UpdateLocalBounds()
{
	FBox LocalBox(ForceInit);

	for (const FVisMeshSection& Section : VisMeshSections)
	{
		LocalBox += Section.SectionLocalBox;
	}

	LocalBounds = LocalBox.IsValid? FBoxSphereBounds(LocalBox): FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0);
	// fallback to reset box sphere bounds

	// Update global bounds
	UpdateBounds();
	// Need to send to render thread
	MarkRenderTransformDirty();
}

void UVisMeshProceduralComponent::CreateVisMeshBodySetup()
{
	if (VisMeshBodySetup == nullptr)
	{
		VisMeshBodySetup = CreateBodySetupHelper();
	}
}

void UVisMeshProceduralComponent::UpdateCollision()
{
	SCOPE_CYCLE_COUNTER(STAT_VisMesh_UpdateCollision);

	UWorld* World = GetWorld();
	const bool bUseAsyncCook = World && World->IsGameWorld() && bUseAsyncCooking;

	if (bUseAsyncCook)
	{
		// Abort all previous ones still standing
		for (UBodySetup* OldBody : AsyncBodySetupQueue)
		{
			OldBody->AbortPhysicsMeshAsyncCreation();
		}

		AsyncBodySetupQueue.Add(CreateBodySetupHelper());
	}
	else
	{
		AsyncBodySetupQueue.Empty();
		//If for some reason we modified the async at runtime, just clear any pending async body setups
		CreateVisMeshBodySetup();
	}

	UBodySetup* UseBodySetup = bUseAsyncCook ? AsyncBodySetupQueue.Last() : VisMeshBodySetup;

	// Fill in simple collision convex elements
	UseBodySetup->AggGeom.ConvexElems = CollisionConvexElems;

	// Set trace flag
	UseBodySetup->CollisionTraceFlag = bUseComplexAsSimpleCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;

	if (bUseAsyncCook)
	{
		UseBodySetup->CreatePhysicsMeshesAsync(FOnAsyncPhysicsCookFinished::CreateUObject(this, &UVisMeshProceduralComponent::FinishPhysicsAsyncCook, UseBodySetup));
	}
	else
	{
		// New GUID as collision has changed
		UseBodySetup->BodySetupGuid = FGuid::NewGuid();
		// Also we want cooked data for this
		UseBodySetup->bHasCookedCollisionData = true;
		UseBodySetup->InvalidatePhysicsData();
		UseBodySetup->CreatePhysicsMeshes();
		RecreatePhysicsState();
	}
}

void UVisMeshProceduralComponent::FinishPhysicsAsyncCook(bool bSuccess, UBodySetup* FinishedBodySetup)
{
	TArray<UBodySetup*> NewQueue;
	NewQueue.Reserve(AsyncBodySetupQueue.Num());

	int32 FoundIdx;
	if (AsyncBodySetupQueue.Find(FinishedBodySetup, FoundIdx))
	{
		if (bSuccess)
		{
			//The new body was found in the array meaning it's newer so use it
			VisMeshBodySetup = FinishedBodySetup;
			RecreatePhysicsState();

			//remove any async body setups that were requested before this one
			for (int32 AsyncIdx = FoundIdx + 1; AsyncIdx < AsyncBodySetupQueue.Num(); ++AsyncIdx)
			{
				NewQueue.Add(AsyncBodySetupQueue[AsyncIdx]);
			}

			AsyncBodySetupQueue = NewQueue;
		}
		else
		{
			AsyncBodySetupQueue.RemoveAt(FoundIdx);
		}
	}
}

UBodySetup* UVisMeshProceduralComponent::CreateBodySetupHelper()
{
	// The body setup in a template needs to be public since the property is Tnstanced and thus is the archetype of the instance meaning there is a direct reference
	UBodySetup* NewBodySetup = NewObject<UBodySetup>(this, NAME_None,(IsTemplate() ? RF_Public | RF_ArchetypeObject : RF_NoFlags));
	NewBodySetup->BodySetupGuid = FGuid::NewGuid();

	NewBodySetup->bGenerateMirroredCollision = false;
	NewBodySetup->bDoubleSidedGeometry = true;
	NewBodySetup->CollisionTraceFlag = bUseComplexAsSimpleCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;

	return NewBodySetup;
}
