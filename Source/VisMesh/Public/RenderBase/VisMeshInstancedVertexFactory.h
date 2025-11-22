#pragma once
#include "CoreMinimal.h"
#include "LocalVertexFactory.h"
#include "VisMeshRenderResources.h"

// 定义与 Shader 中 InstanceVF 对应的 Uniform Buffer 结构
// 对应 Shader 代码: UniformBuffer InstanceVF;
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FInstancedVisMeshVertexFactoryUniformShaderParameters, )
	SHADER_PARAMETER_SRV(Buffer<float4>, VertexFetch_InstanceOriginBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, VertexFetch_InstanceTransformBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, VertexFetch_InstanceLightmapBuffer)
	SHADER_PARAMETER_SRV(Buffer<float>, InstanceCustomDataBuffer)
	SHADER_PARAMETER(int32, NumCustomDataFloats)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

// 这个 Loose 参数通常用于剔除 (Culling)，如果暂时没用到剔除逻辑可以先保留
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FInstancedVisMeshVFLooseUniformShaderParameters, )
	SHADER_PARAMETER(FVector4f, InstancingViewZCompareZero)
	SHADER_PARAMETER(FVector4f, InstancingViewZCompareOne)
	SHADER_PARAMETER(FVector4f, InstancingViewZConstant)
	SHADER_PARAMETER(FVector4f, InstancingTranslatedWorldViewOriginZero)
	SHADER_PARAMETER(FVector4f, InstancingTranslatedWorldViewOriginOne)
	SHADER_PARAMETER(FVector4f, InstancingFadeOutParams)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FInstancedVisMeshVFLooseUniformShaderParameters> FInstancedVisMeshVFLooseUniformShaderParametersRef;

/**
 * 自定义顶点工厂，继承自 FLocalVertexFactory
 */
class FVisMeshInstancedVertexFactory : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FMyInstancedVertexFactory);

public:
	FVisMeshInstancedVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const char* InDebugName)
		: FLocalVertexFactory(InFeatureLevel, InDebugName)
	{}

	// 在这里根据传入的 SRV 构建 Uniform Buffer
	void SetData(const FDataType& InData, const FInstancedVisMeshDataType* InInstanceData)
	{
		Data = InData;
		if (InInstanceData)
		{
			InstanceData = *InInstanceData;
		}
		UpdateRHI(FRenderResource::GetCommandList());
	}
	
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	// 供 ShaderParameters 类获取 UniformBuffer
	FRHIUniformBuffer* GetInstanceUniformBuffer() const
	{
		return UniformBuffer;
	}

	void GetVertexElements(ERHIFeatureLevel::Type InFeatureLevel, 
		EVertexInputStreamType InputStreamType, 
		bool bSupportsManualVertexFetch, 
		FDataType& InData, 
		FInstancedVisMeshDataType& InInstanceData, 
		FVertexDeclarationElementList& Elements);
	
	// SRV Accessors for ShaderBindings
	FRHIShaderResourceView* GetInstanceOriginSRV() const { return InstanceData.InstanceOriginSRV; }
	FRHIShaderResourceView* GetInstanceTransformSRV() const { return InstanceData.InstanceTransformSRV; }
	FRHIShaderResourceView* GetInstanceLightmapSRV() const { return InstanceData.InstanceLightmapSRV; }
	FRHIShaderResourceView* GetInstanceCustomDataSRV() const { return InstanceData.InstanceCustomDataSRV; }

protected:
	// The core function that maps C++ streams to HLSL Attributes
	static void GetVertexElements(ERHIFeatureLevel::Type InFeatureLevel, 
		EVertexInputStreamType InputStreamType, 
		bool bSupportsManualVertexFetch, 
		FDataType& InData, 
		FInstancedVisMeshDataType& InInstanceData, 
		FVertexDeclarationElementList& Elements, 
		FVertexStreamList& Streams);
private:
	FInstancedVisMeshDataType InstanceData;

	TUniformBufferRef<FInstancedVisMeshVertexFactoryUniformShaderParameters> UniformBuffer;
};

// 定义 Shader Parameter 绑定类
// 负责将 C++ 的 UniformBuffer 绑定到 HLSL 的 InstanceVF
class FVisMeshInstancedVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FVisMeshInstancedVertexFactoryShaderParameters, NonVirtual);
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		// FLocalVertexFactoryShaderParametersBase::Bind(ParameterMap);

		InstancingOffsetParameter.Bind(ParameterMap, TEXT("InstancingOffset"));
		VertexFetch_InstanceOriginBufferParameter.Bind(ParameterMap, TEXT("VertexFetch_InstanceOriginBuffer"));
		VertexFetch_InstanceTransformBufferParameter.Bind(ParameterMap, TEXT("VertexFetch_InstanceTransformBuffer"));
		VertexFetch_InstanceLightmapBufferParameter.Bind(ParameterMap, TEXT("VertexFetch_InstanceLightmapBuffer"));
		InstanceOffset.Bind(ParameterMap, TEXT("InstanceOffset"));
	}

	void GetElementShaderBindings(
		const FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams
	) const;
private:
	LAYOUT_FIELD(FShaderParameter, InstancingOffsetParameter);
	LAYOUT_FIELD(FShaderResourceParameter, VertexFetch_InstanceOriginBufferParameter)
	LAYOUT_FIELD(FShaderResourceParameter, VertexFetch_InstanceTransformBufferParameter)
	LAYOUT_FIELD(FShaderResourceParameter, VertexFetch_InstanceLightmapBufferParameter)
	LAYOUT_FIELD(FShaderParameter, InstanceOffset)
};