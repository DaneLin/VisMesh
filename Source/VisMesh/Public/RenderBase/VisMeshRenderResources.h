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

	FPositionUAVVertexBuffer(int32 InNumVertices)
		:NumVertices(InNumVertices)
	{
	}

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
struct FInstancedVisMeshDataType
{
	/** The stream to read the mesh transform from. */
	FVertexStreamComponent InstanceOriginComponent;

	/** The stream to read the mesh transform from. */
	FVertexStreamComponent InstanceTransformComponent[3];

	/** The stream to read the Lightmap Bias and Random instance ID from. */
	FVertexStreamComponent InstanceLightmapAndShadowMapUVBiasComponent;

	FRHIShaderResourceView* InstanceOriginSRV = nullptr;
	FRHIShaderResourceView* InstanceTransformSRV = nullptr;
	FRHIShaderResourceView* InstanceLightmapSRV = nullptr;
	FRHIShaderResourceView* InstanceCustomDataSRV = nullptr;

	int32 NumCustomDataFloats = 0;
};

class FVisMeshSubBuffer : public FVertexBuffer
{
public:
	FVisMeshSubBuffer(uint32 InVector4CountPerInstance) 
		: Vector4CountPerInstance(InVector4CountPerInstance)
		, NumInstances(0)
	{}

	void Init(int32 InNumInstances)
	{
		NumInstances = InNumInstances;
	}

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	FRHIShaderResourceView* GetSRV() const { return SRV; }
	FRHIUnorderedAccessView* GetUAV() const { return UAV; }

private:
	uint32 Vector4CountPerInstance;
	int32 NumInstances;
	FShaderResourceViewRHIRef SRV;
	FUnorderedAccessViewRHIRef UAV;
};

/** * 管理类：组合了 Origin, Transform, Lightmap 三个独立的 Buffer
 * 注意：这里不再继承 FVertexBuffer，而是继承 FRenderResource，因为它管理着多个 VertexBuffer
 */
class FVisMeshInstanceBuffer : public FRenderResource
{
public:
	FVisMeshInstanceBuffer(int32 InNumInstances)
		: OriginBuffer(1)    // 1x float4
		, TransformBuffer(3) // 3x float4
		, LightmapBuffer(1)  // 1x float4
		, NumInstances(InNumInstances)
	{
		// 预先设置好实例数量
		OriginBuffer.Init(NumInstances);
		TransformBuffer.Init(NumInstances);
		LightmapBuffer.Init(NumInstances);
	}

	virtual const TCHAR* GetName() const { return TEXT("FVisMeshInstanceBuffer"); }

	// 依次初始化所有子 Buffer
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		OriginBuffer.InitResource(RHICmdList);
		TransformBuffer.InitResource(RHICmdList);
		LightmapBuffer.InitResource(RHICmdList);
	}

	// 依次释放所有子 Buffer
	virtual void ReleaseRHI() override
	{
		OriginBuffer.ReleaseResource();
		TransformBuffer.ReleaseResource();
		LightmapBuffer.ReleaseResource();
	}

	void BindToDataType(FInstancedVisMeshDataType& OutData) const;

	// 获取 Accessors (Compute Shader 需要分别绑定这三个 UAV)
	FRHIUnorderedAccessView* GetOriginUAV() const { return OriginBuffer.GetUAV(); }
	FRHIUnorderedAccessView* GetTransformUAV() const { return TransformBuffer.GetUAV(); }
	FRHIUnorderedAccessView* GetLightmapUAV() const { return LightmapBuffer.GetUAV(); }

	int32 GetNumInstances() const { return NumInstances; }

private:
	// 子资源
	FVisMeshSubBuffer OriginBuffer;
	FVisMeshSubBuffer TransformBuffer;
	FVisMeshSubBuffer LightmapBuffer;

	int32 NumInstances;
};