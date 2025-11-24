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

/** 
 * 纯数据容器，采用 SOA 布局,用于在 API 间传递网格数据 
 */
USTRUCT(BlueprintType)
struct FVisMeshData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector> Positions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector> Normals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVisMeshTangent> Tangents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FColor> Colors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector2D> UV0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector2D> UV1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector2D> UV2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector2D> UV3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<int32> Triangles; // 这里用 int32 方便蓝图，内部转 uint32

	/** 辅助函数：快速检查数据有效性 */
	bool IsValid() const { return Positions.Num() > 0; }
	int32 NumVertices() const { return Positions.Num(); }

	/** 辅助函数：清理数据 */
	void Reset()
	{
		Positions.Reset();
		Normals.Reset();
		Tangents.Reset();
		Colors.Reset();
		UV0.Reset();
		UV1.Reset();
		UV2.Reset();
		UV3.Reset();
		Triangles.Reset();
	}
};

USTRUCT()
struct FVisMeshSection
{
	GENERATED_BODY()

	// 直接复用数据结构
	UPROPERTY()
	FVisMeshData Data; 

	// --- 仅保留状态数据 ---
	UPROPERTY()
	FBox SectionLocalBox;

	UPROPERTY()
	bool bEnableCollision = false;

	UPROPERTY()
	bool bSectionVisible = true;
    
	FVisMeshSection() : SectionLocalBox(ForceInit) {}

	void Reset()
	{
		Data.Reset();
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
	FVisMeshData Data;
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