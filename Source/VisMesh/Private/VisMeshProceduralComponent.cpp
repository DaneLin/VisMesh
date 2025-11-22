// Copyright ZJU CAD. All Rights Reserved.

#include "VisMeshProceduralComponent.h"

#include "VisMeshRenderResources.h"
#include "VisMeshSceneProxy.h"
#include "PhysicsEngine/BodySetup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VisMeshProceduralComponent)

UVisMeshProceduralComponent::UVisMeshProceduralComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseComplexAsSimpleCollision = true;
}

void UVisMeshProceduralComponent::CreateMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices,const TArray<int32>& Triangles, const TArray<FVector>& Normals,const TArray<FVector2D>& UV0,const TArray<FVector2D>& UV1, const TArray<FVector2D>& UV2,const TArray<FVector2D>& UV3,const TArray<FColor>& VertexColors, const TArray<FVisMeshTangent>& Tangents,bool bCreateCollision)
{
	SCOPE_CYCLE_COUNTER(STAT_VisMesh_CreateMeshSection);

	// Ensure sections array is long enough
	if (SectionIndex >= VisMeshSections.Num())
	{
		VisMeshSections.SetNum(SectionIndex + 1, false);
	}

	// Reset this section (in case it already existed)
	FVisMeshSection& NewSection = VisMeshSections[SectionIndex];
	NewSection.Reset();

	// Copy data to vertex buffer
	const int32 NumVerts = Vertices.Num();
	NewSection.ProcVertexBuffer.Reset();
	NewSection.ProcVertexBuffer.AddUninitialized(NumVerts);
	for (int32 VertIdx = 0; VertIdx < NumVerts; VertIdx++)
	{
		FVisMeshVertex& Vertex = NewSection.ProcVertexBuffer[VertIdx];

		Vertex.Position = Vertices[VertIdx];
		Vertex.Normal = (Normals.Num() == NumVerts) ? Normals[VertIdx] : FVector(0.f, 0.f, 1.f);
		Vertex.UV0 = (UV0.Num() == NumVerts) ? UV0[VertIdx] : FVector2D(0.f, 0.f);
		Vertex.UV1 = (UV1.Num() == NumVerts) ? UV1[VertIdx] : FVector2D(0.f, 0.f);
		Vertex.UV2 = (UV2.Num() == NumVerts) ? UV2[VertIdx] : FVector2D(0.f, 0.f);
		Vertex.UV3 = (UV3.Num() == NumVerts) ? UV3[VertIdx] : FVector2D(0.f, 0.f);
		Vertex.Color = (VertexColors.Num() == NumVerts) ? VertexColors[VertIdx] : FColor(255, 255, 255);
		Vertex.Tangent = (Tangents.Num() == NumVerts) ? Tangents[VertIdx] : FVisMeshTangent();

		// Update bounding box
		NewSection.SectionLocalBox += Vertex.Position;
	}

	// Get triangle indices, clamping to vertex range
	const int32 MaxIndex = NumVerts - 1;
	const auto GetTriIndices = [&Triangles, MaxIndex](int32 Idx)
	{
		return TTuple<int32, int32, int32>(FMath::Min(Triangles[Idx], MaxIndex),
		                                   FMath::Min(Triangles[Idx + 1], MaxIndex),
		                                   FMath::Min(Triangles[Idx + 2], MaxIndex));
	};

	const int32 NumTriIndices = (Triangles.Num() / 3) * 3; // Ensure number of triangle indices is multiple of three

	// Detect degenerate triangles, i.e. non-unique vertex indices within the same triangle
	int32 NumDegenerateTriangles = 0;
	for (int32 IndexIdx = 0; IndexIdx < NumTriIndices; IndexIdx += 3)
	{
		int32 a, b, c;
		Tie(a, b, c) = GetTriIndices(IndexIdx);
		NumDegenerateTriangles += a == b || a == c || b == c;
	}
	if (NumDegenerateTriangles > 0)
	{
		UE_LOG(LogVisComponent, Warning,
		       TEXT(
			       "Detected %d degenerate triangle%s with non-unique vertex indices for created mesh section in '%s'; degenerate triangles will be dropped."
		       ),
		       NumDegenerateTriangles, NumDegenerateTriangles > 1 ? TEXT("s") : TEXT(""), *GetFullName());
	}

	// Copy index buffer for non-degenerate triangles
	NewSection.ProcIndexBuffer.Reset();
	NewSection.ProcIndexBuffer.AddUninitialized(NumTriIndices - NumDegenerateTriangles * 3);
	int32 CopyIndexIdx = 0;
	for (int32 IndexIdx = 0; IndexIdx < NumTriIndices; IndexIdx += 3)
	{
		int32 a, b, c;
		Tie(a, b, c) = GetTriIndices(IndexIdx);

		if (a != b && a != c && b != c)
		{
			NewSection.ProcIndexBuffer[CopyIndexIdx++] = a;
			NewSection.ProcIndexBuffer[CopyIndexIdx++] = b;
			NewSection.ProcIndexBuffer[CopyIndexIdx++] = c;
		}
		else
		{
			--NumDegenerateTriangles;
		}
	}
	check(NumDegenerateTriangles == 0);
	check(CopyIndexIdx == NewSection.ProcIndexBuffer.Num());

	NewSection.bEnableCollision = bCreateCollision;

	UpdateLocalBounds(); // Update overall bounds
	UpdateCollision(); // Mark collision as dirty
	MarkRenderStateDirty(); // New section requires recreating scene proxy
}

void UVisMeshProceduralComponent::CreateMeshSection_LinearColor(int32 SectionIndex, const TArray<FVector>& Vertices,const TArray<int32>& Triangles, const TArray<FVector>& Normals,const TArray<FVector2D>& UV0,const TArray<FVector2D>& UV1, const TArray<FVector2D>& UV2,const TArray<FVector2D>& UV3,const TArray<FLinearColor>& VertexColors,const TArray<FVisMeshTangent>& Tangents, bool bCreateCollision,bool bSRGBConversion)
{
	// Convert FLinearColors to FColors
	TArray<FColor> Colors;
	if (VertexColors.Num() > 0)
	{
		Colors.SetNum(VertexColors.Num());

		for (int32 ColorIdx = 0; ColorIdx < VertexColors.Num(); ColorIdx++)
		{
			Colors[ColorIdx] = VertexColors[ColorIdx].ToFColor(bSRGBConversion);
		}
	}

	CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UV0, UV1, UV2, UV3, Colors, Tangents, bCreateCollision);
}

void UVisMeshProceduralComponent::UpdateMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices,const TArray<FVector>& Normals, const TArray<FVector2D>& UV0,const TArray<FVector2D>& UV1,const TArray<FVector2D>& UV2, const TArray<FVector2D>& UV3,const TArray<FColor>& VertexColors,const TArray<FVisMeshTangent>& Tangents)
{
	SCOPE_CYCLE_COUNTER(STAT_VisMesh_UpdateSectionGT);

	if (SectionIndex < VisMeshSections.Num())
	{
		FVisMeshSection& Section = VisMeshSections[SectionIndex];
		const int32 NumVerts = Vertices.Num();
		const int32 PreviousNumVerts = Section.ProcVertexBuffer.Num();

		// See if positions are changing
		const bool bSameVertexCount = PreviousNumVerts == NumVerts;

		if (bSameVertexCount)
		{
			Section.SectionLocalBox = Vertices.Num() ? FBox(Vertices) : FBox(ForceInit);

			// Iterate through vertex data, copying in new info
			for (int32 VertIdx = 0; VertIdx < NumVerts; ++VertIdx)
			{
				FVisMeshVertex& ModifyVert = Section.ProcVertexBuffer[VertIdx];

				// Position data
				if (Vertices.Num() == NumVerts)
				{
					ModifyVert.Position = Vertices[VertIdx];
				}

				// Normal data
				if (Normals.Num() == NumVerts)
				{
					ModifyVert.Normal = Normals[VertIdx];
				}

				// Tangent data
				if (Tangents.Num() == NumVerts)
				{
					ModifyVert.Tangent = Tangents[VertIdx];
				}

				// UV0 data
				if (UV0.Num() == NumVerts)
				{
					ModifyVert.UV0 = UV0[VertIdx];
				}
				// UV1 data
				if (UV1.Num() == NumVerts)
				{
					ModifyVert.UV1 = UV1[VertIdx];
				}
				// UV2 data
				if (UV2.Num() == NumVerts)
				{
					ModifyVert.UV2 = UV2[VertIdx];
				}
				// UV3 data
				if (UV3.Num() == NumVerts)
				{
					ModifyVert.UV3 = UV3[VertIdx];
				}

				// Color data
				if (VertexColors.Num() == NumVerts)
				{
					ModifyVert.Color = VertexColors[VertIdx];
				}
			}

			// If we have collision enabled on this section, update that too
			if (Section.bEnableCollision)
			{
				TArray<FVector> CollisionPositions;

				// We have one collision mesh for all sections, so need to build array of _all_ positions
				for (const FVisMeshSection& CollisionSection : VisMeshSections)
				{
					// If section has collision, copy it
					if (CollisionSection.bEnableCollision)
					{
						for (int32 VertIdx = 0; VertIdx < CollisionSection.ProcVertexBuffer.Num(); VertIdx++)
						{
							CollisionPositions.Add(CollisionSection.ProcVertexBuffer[VertIdx].Position);
						}
					}
				}

				// Pass new positions to trimesh
				BodyInstance.UpdateTriMeshVertices(CollisionPositions);
			}

			// If we have a valid proxy and it is not pending recreation
			if (SceneProxy && !IsRenderStateDirty())
			{
				// Create data to update section
				FVisMeshSectionUpdateData* SectionData = new FVisMeshSectionUpdateData;
				SectionData->TargetSection = SectionIndex;
				SectionData->NewVertexBuffer = Section.ProcVertexBuffer;

				// // Enqueue command to send to render thread
				FVisMeshSceneProxy* ProcMeshSceneProxy = (FVisMeshSceneProxy*)SceneProxy;
				ENQUEUE_RENDER_COMMAND(FVisMeshSectionUpdate)
				([ProcMeshSceneProxy, SectionData](FRHICommandListImmediate& RHICmdList)
				{
					ProcMeshSceneProxy->UpdateSection_RenderThread(RHICmdList, SectionData);
				});
				
			}

			UpdateLocalBounds(); // Update overall bounds
			MarkRenderTransformDirty(); // Need to send new bounds to render thread
		}
		else
		{
			UE_LOG(LogVisComponent, Error,TEXT("Trying to update a procedural mesh component section with a different number of vertices [Previous: %i, New: %i] (clear and recreate mesh section instead)"), PreviousNumVerts, NumVerts);
		}
	}
}

void UVisMeshProceduralComponent::UpdateMeshSection_LinearColor(int32 SectionIndex, const TArray<FVector>& Vertices,const TArray<FVector>& Normals, const TArray<FVector2D>& UV0,const TArray<FVector2D>& UV1,const TArray<FVector2D>& UV2, const TArray<FVector2D>& UV3,const TArray<FLinearColor>& VertexColors,const TArray<FVisMeshTangent>& Tangents, bool bSRGBConversion)
{
	// Convert FLinearColors to FColors
	TArray<FColor> Colors;
	if (VertexColors.Num() > 0)
	{
		Colors.SetNum(VertexColors.Num());

		for (int32 ColorIdx = 0; ColorIdx < VertexColors.Num(); ColorIdx++)
		{
			Colors[ColorIdx] = VertexColors[ColorIdx].ToFColor(bSRGBConversion);
		}
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
			FVisMeshSceneProxy* ProcMeshSceneProxy = (FVisMeshSceneProxy*)SceneProxy;
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
	return IInterface_CollisionDataProvider::GetPhysicsTriMeshData(CollisionData, InUseAllTriData);
}

bool UVisMeshProceduralComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	return IInterface_CollisionDataProvider::ContainsPhysicsTriMeshData(InUseAllTriData);
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

void UVisMeshProceduralComponent::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property)
	{
		return;
	}

	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UVisMeshProceduralComponent, bUseInstance))
	{
		MarkRenderStateDirty();
	}
}

FPrimitiveSceneProxy* UVisMeshProceduralComponent::CreateSceneProxy()
{
	SCOPE_CYCLE_COUNTER(STAT_VisMesh_CreateSceneProxy);
	
	return new FVisMeshSceneProxy(this);
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
		// Look for element that corresponds to the supplied face
		int32 TotalFaceCount = 0;
		for (int32 SectionIdx = 0; SectionIdx < VisMeshSections.Num(); SectionIdx++)
		{
			const FVisMeshSection& Section = VisMeshSections[SectionIdx];
			int32 NumFaces = Section.ProcIndexBuffer.Num() / 3;
			TotalFaceCount += NumFaces;

			if (FaceIndex < TotalFaceCount)
			{
				// Grab the material
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
