#pragma once
#include "DynamicMeshBuilder.h"
#include "VisMeshRenderResources.generated.h"

/** Class representing a single section of the proc mesh */
class FVisMeshProxySection
{
public:
	/** Material applied to this section */
	UMaterialInterface* Material;
	/** Vertex buffer for this section */
	FStaticMeshVertexBuffers VertexBuffers;
	/** Index buffer for this section */
	FDynamicMeshIndexBuffer32 IndexBuffer;
	/** Vertex factory for this section */
	FLocalVertexFactory VertexFactory;
	/** Whether this section is currently visible */
	bool bSectionVisible;

	FVisMeshProxySection(ERHIFeatureLevel::Type InFeatureLevel)
		: Material(NULL)
		  , VertexFactory(InFeatureLevel, "FVisMeshProxySection")
		  , bSectionVisible(true)
	{
	}
};

/**
*	Note: Codes from UvisMeshComponent.h
*	Struct used to specify a tangent vector for a vertex
*	The Y tangent is computed from the cross product of the vertex normal (Tangent Z) and the TangentX member.
*/
USTRUCT(BlueprintType)
struct FVisMeshTangent
{
	GENERATED_BODY()
public:

	/** Direction of X tangent for this vertex */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tangent)
	FVector TangentX;

	/** Bool that indicates whether we should flip the Y tangent when we compute it using cross product */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tangent)
	bool bFlipTangentY;

	FVisMeshTangent()
		: TangentX(1.f, 0.f, 0.f)
		, bFlipTangentY(false)
	{}

	FVisMeshTangent(float X, float Y, float Z)
		: TangentX(X, Y, Z)
		, bFlipTangentY(false)
	{}

	FVisMeshTangent(FVector InTangentX, bool bInFlipTangentY)
		: TangentX(InTangentX)
		, bFlipTangentY(bInFlipTangentY)
	{}
};

/** One vertex for the vis mesh, used for storing data internally */
USTRUCT(BlueprintType)
struct FVisMeshVertex
{
	GENERATED_BODY()
public:

	/** Vertex position */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FVector Position;

	/** Vertex normal */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FVector Normal;

	/** Vertex tangent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FVisMeshTangent Tangent;

	/** Vertex color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FColor Color;

	/** Vertex texture co-ordinate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FVector2D UV0;

	/** Vertex texture co-ordinate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FVector2D UV1;

	/** Vertex texture co-ordinate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FVector2D UV2;

	/** Vertex texture co-ordinate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FVector2D UV3;


	FVisMeshVertex()
		: Position(0.f, 0.f, 0.f)
		, Normal(0.f, 0.f, 1.f)
		, Tangent(FVector(1.f, 0.f, 0.f), false)
		, Color(255, 255, 255)
		, UV0(0.f, 0.f)
		, UV1(0.f, 0.f)
		, UV2(0.f, 0.f)
		, UV3(0.f, 0.f)
	{}
};

/** One section of the vis mesh. Each material has its own section. */
USTRUCT()
struct FVisMeshSection
{
	GENERATED_BODY()
public:

	/** Vertex buffer for this section */
	UPROPERTY()
	TArray<FVisMeshVertex> ProcVertexBuffer;

	/** Index buffer for this section */
	UPROPERTY()
	TArray<uint32> ProcIndexBuffer;
	/** Local bounding box of section */
	UPROPERTY()
	FBox SectionLocalBox;

	/** Should we build collision data for triangles in this section */
	UPROPERTY()
	bool bEnableCollision;

	/** Should we display this section */
	UPROPERTY()
	bool bSectionVisible;

	FVisMeshSection()
		: SectionLocalBox(ForceInit)
		, bEnableCollision(false)
		, bSectionVisible(true)
	{}

	/** Reset this section, clear all mesh info. */
	void Reset()
	{
		ProcVertexBuffer.Empty();
		ProcIndexBuffer.Empty();
		SectionLocalBox.Init();
		bEnableCollision = false;
		bSectionVisible = true;
	}
};


class FVisMeshSectionUpdateData
{
public:
	/** Section to update */
	int32 TargetSection;
	/** New vertex information */
	TArray<FVisMeshVertex> NewVertexBuffer;
};

class FPositionUAVVertexBuffer : public FVertexBuffer
{
public:
	virtual const TCHAR* GetName() const;

	FPositionUAVVertexBuffer(int32 InNumVertices);

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	virtual void ReleaseRHI() override;

	FRHIShaderResourceView* GetSRV() const { return SRV; }
	FRHIUnorderedAccessView* GetUAV() const { return UAV; }
private:
	int32 NumVertices;
	FShaderResourceViewRHIRef SRV;
	FUnorderedAccessViewRHIRef UAV;
};

// 对应 LocalVertexFactory.ush 中的 Attributes 8-12
struct FVisMeshInstanceData
{
	// ATTRIBUTE8: Origin (xyz), Random (w)
	FVector4f InstanceOrigin; 
    
	// ATTRIBUTE9: Transform Row 0 (xyz), HitProxy/Selected (w)
	FVector4f InstanceTransform1; 
    
	// ATTRIBUTE10: Transform Row 1 (xyz), HitProxy (w)
	FVector4f InstanceTransform2; 
    
	// ATTRIBUTE11: Transform Row 2 (xyz), HitProxy (w)
	FVector4f InstanceTransform3; 
    
	// ATTRIBUTE12: Lightmap/Shadowmap UV Bias (xy, zw)
	FVector4f InstanceLightmapAndShadowMapUVBias; 
};

