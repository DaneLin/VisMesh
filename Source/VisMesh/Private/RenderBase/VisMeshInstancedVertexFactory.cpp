#include "RenderBase/VisMeshInstancedVertexFactory.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "MeshDrawShaderBindings.h"
#include "MeshMaterialShader.h"
#include "Engine/InstancedStaticMesh.h"

const int32 InstancedVisMeshMaxTexCoord = 8;

IMPLEMENT_TYPE_LAYOUT(FVisMeshInstancedVertexFactoryShaderParameters);

// Bind the C++ parameter struct to the HLSL name "VisMeshInstanceVF"
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FInstancedVisMeshVertexFactoryUniformShaderParameters, "VisMeshInstanceVF");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FInstancedVisMeshVFLooseUniformShaderParameters, "VisMeshInstancedVFLooseParameters");

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FVisMeshInstancedVertexFactory, SF_Vertex, FVisMeshInstancedVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FVisMeshInstancedVertexFactory, SF_Pixel, FVisMeshInstancedVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_TYPE(FVisMeshInstancedVertexFactory, 
	"/VisMeshPlugin/CommonBase/VisMeshLocalVertexFactory.ush",
	EVertexFactoryFlags::UsedWithMaterials 
	| EVertexFactoryFlags::SupportsDynamicLighting 
	| EVertexFactoryFlags::SupportsManualVertexFetch
);

void FVisMeshInstancedVertexFactory::ModifyCompilationEnvironment(
	const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FLocalVertexFactory::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        
	// 开启 Instancing 宏，激活 USH 中的逻辑
	OutEnvironment.SetDefine(TEXT("USE_INSTANCING"), TEXT("1"));
		
	// Ensure MVF is enabled if the platform supports it
	if (RHISupportsManualVertexFetch(Parameters.Platform))
	{
		OutEnvironment.SetDefine(TEXT("MANUAL_VERTEX_FETCH"), TEXT("1"));
	}
}

bool FVisMeshInstancedVertexFactory::ShouldCompilePermutation(
	const FVertexFactoryShaderPermutationParameters& Parameters)
{
	// 确保只在支持 Manual Vertex Fetch 的平台上启用
	return RHISupportsManualVertexFetch(Parameters.Platform); 
}

void FVisMeshInstancedVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
{
	check(HasValidFeatureLevel());

	FLocalVertexFactory::InitRHI(RHICmdList);
	
	{
		FInstancedVisMeshVertexFactoryUniformShaderParameters UniformParameters;
		UniformParameters.VertexFetch_InstanceOriginBuffer = GetInstanceOriginSRV();
		UniformParameters.VertexFetch_InstanceTransformBuffer = GetInstanceTransformSRV();
		UniformParameters.VertexFetch_InstanceLightmapBuffer = GetInstanceLightmapSRV();
		UniformParameters.InstanceCustomDataBuffer = GetInstanceCustomDataSRV();
		UniformParameters.NumCustomDataFloats = InstanceData.NumCustomDataFloats;
		InstanceBuffer = TUniformBufferRef<FInstancedVisMeshVertexFactoryUniformShaderParameters>::CreateUniformBufferImmediate(UniformParameters, UniformBuffer_MultiFrame, EUniformBufferValidation::None);
	}

	{
		FInstancedVisMeshVFLooseUniformShaderParameters LooseParams;
		// 设置默认值，防止 Shader 计算出错 (0/0 或 无限大)
		// FadeOutParams: x=StartDistance, y=InvFadeRange, z=RenderSelected, w=RenderDeselected
		// 下面的设置意味着：不淡出，始终可见
		LooseParams.InstancingFadeOutParams = FVector4f(0.0f, 0.0f, 1.0f, 1.0f); 
        
		// ViewOrigin 和其他参数初始化为 0
		LooseParams.InstancingTranslatedWorldViewOriginZero = FVector4f(0,0,0,0);
		LooseParams.InstancingTranslatedWorldViewOriginOne = FVector4f(0,0,0,0);
		LooseParams.InstancingViewZCompareZero = FVector4f(0,0,0,0);
		LooseParams.InstancingViewZCompareOne = FVector4f(0,0,0,0);
		LooseParams.InstancingViewZConstant = FVector4f(0,0,0,0);

		VisMeshLooseParametersUniformBuffer = TUniformBufferRef<FInstancedVisMeshVFLooseUniformShaderParameters>::CreateUniformBufferImmediate(
			LooseParams, 
			UniformBuffer_MultiFrame
		);
	}
}

void FVisMeshInstancedVertexFactory::GetVertexElements(ERHIFeatureLevel::Type InFeatureLevel,
	EVertexInputStreamType InputStreamType, bool bSupportsManualVertexFetch, FDataType& InData,
	FInstancedVisMeshDataType& InInstanceData, FVertexDeclarationElementList& Elements)
{
	FVertexStreamList VertexStreams;
	GetVertexElements(InFeatureLevel, InputStreamType, bSupportsManualVertexFetch, InData, InInstanceData, Elements, VertexStreams);
}

void FVisMeshInstancedVertexFactory::GetVertexElements(ERHIFeatureLevel::Type InFeatureLevel,
	EVertexInputStreamType InputStreamType, bool bSupportsManualVertexFetch, FDataType& InData,
	FInstancedVisMeshDataType& InInstanceData, FVertexDeclarationElementList& Elements, FVertexStreamList& Streams)
{
	if (InData.PositionComponent.VertexBuffer != NULL)
	{
		Elements.Add(AccessStreamComponent(InData.PositionComponent, 0, Streams));
	}

	if (!bSupportsManualVertexFetch)
	{
		// only tangent,normal are used by the stream. the binormal is derived in the shader
		uint8 TangentBasisAttributes[2] = { 1, 2 };
		for (int32 AxisIndex = 0; AxisIndex < 2; AxisIndex++)
		{
			if (InData.TangentBasisComponents[AxisIndex].VertexBuffer != NULL)
			{
				Elements.Add(AccessStreamComponent(InData.TangentBasisComponents[AxisIndex], TangentBasisAttributes[AxisIndex], Streams));
			}
		}

		if (InData.ColorComponentsSRV == nullptr)
		{
			InData.ColorComponentsSRV = GNullColorVertexBuffer.VertexBufferSRV;
			InData.ColorIndexMask = 0;
		}

		if (InData.ColorComponent.VertexBuffer)
		{
			Elements.Add(AccessStreamComponent(InData.ColorComponent, 3, Streams));
		}
		else
		{
			//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
			//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
			FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color, EVertexStreamUsage::ManualFetch);
			Elements.Add(AccessStreamComponent(NullColorComponent, 3, Streams));
		}

		if (InData.TextureCoordinates.Num())
		{
			const int32 BaseTexCoordAttribute = 4;
			for (int32 CoordinateIndex = 0; CoordinateIndex < InData.TextureCoordinates.Num(); CoordinateIndex++)
			{
				Elements.Add(AccessStreamComponent(
					InData.TextureCoordinates[CoordinateIndex],
					BaseTexCoordAttribute + CoordinateIndex,
					Streams
				));
			}

			for (int32 CoordinateIndex = InData.TextureCoordinates.Num(); CoordinateIndex < (InstancedVisMeshMaxTexCoord + 1) / 2; CoordinateIndex++)
			{
				Elements.Add(AccessStreamComponent(
					InData.TextureCoordinates[InData.TextureCoordinates.Num() - 1],
					BaseTexCoordAttribute + CoordinateIndex,
					Streams
				));
			}
		}

		// PreSkinPosition attribute is only used for GPUSkinPassthrough variation of local vertex factory.
		// It is not used by ISM so fill with dummy buffer.
		if (FLocalVertexFactory::IsGPUSkinPassThroughSupported(GMaxRHIShaderPlatform))
		{
			FVertexStreamComponent NullComponent(&GNullVertexBuffer, 0, 0, VET_Float4);
			Elements.Add(AccessStreamComponent(NullComponent, 14, Streams));
		}

		if (InData.LightMapCoordinateComponent.VertexBuffer)
		{
			Elements.Add(AccessStreamComponent(InData.LightMapCoordinateComponent, 15, Streams));
		}
		else if (InData.TextureCoordinates.Num())
		{
			Elements.Add(AccessStreamComponent(InData.TextureCoordinates[0], 15, Streams));
		}
	}
	
	if (InFeatureLevel > ERHIFeatureLevel::ES3_1)
	{
		if (InInstanceData.InstanceOriginComponent.VertexBuffer)
		{
			Elements.Add(AccessStreamComponent(InInstanceData.InstanceOriginComponent, 8, Streams));
		}

		if (InInstanceData.InstanceTransformComponent[0].VertexBuffer)
		{
			Elements.Add(AccessStreamComponent(InInstanceData.InstanceTransformComponent[0], 9, Streams));
			Elements.Add(AccessStreamComponent(InInstanceData.InstanceTransformComponent[1], 10, Streams));
			Elements.Add(AccessStreamComponent(InInstanceData.InstanceTransformComponent[2], 11, Streams));
		}

		if (InInstanceData.InstanceLightmapAndShadowMapUVBiasComponent.VertexBuffer)
		{
			Elements.Add(AccessStreamComponent(InInstanceData.InstanceLightmapAndShadowMapUVBiasComponent, 12, Streams));
		}
	}
}


void FVisMeshInstancedVertexFactoryShaderParameters::GetElementShaderBindings(const FSceneInterface* Scene,
                                                                              const FSceneView* View, const FMeshMaterialShader* Shader, const EVertexInputStreamType InputStreamType,
                                                                              ERHIFeatureLevel::Type FeatureLevel, const FVertexFactory* VertexFactory, const FMeshBatchElement& BatchElement,
                                                                              FMeshDrawSingleShaderBindings& ShaderBindings, FVertexInputStreamArray& VertexStreams) const
{
	// Decode VertexFactoryUserData as VertexFactoryUniformBuffer
	FRHIUniformBuffer* VertexFactoryUniformBuffer = static_cast<FRHIUniformBuffer*>(BatchElement.VertexFactoryUserData);
	const auto* LocalVertexFactory = static_cast<const FLocalVertexFactory*>(VertexFactory);
	
	if (LocalVertexFactory->SupportsManualVertexFetch(FeatureLevel))
	{
		if (!VertexFactoryUniformBuffer)
		{
			// No batch element override
			VertexFactoryUniformBuffer = LocalVertexFactory->GetUniformBuffer();
		}

		ShaderBindings.Add(Shader->GetUniformBufferParameter<FLocalVertexFactoryUniformShaderParameters>(), VertexFactoryUniformBuffer);
	}

	//@todo - allow FMeshBatch to supply vertex streams (instead of requiring that they come from the vertex factory), and this userdata hack will no longer be needed for override vertex color
	if (BatchElement.bUserDataIsColorVertexBuffer)
	{
		FColorVertexBuffer* OverrideColorVertexBuffer = (FColorVertexBuffer*)BatchElement.UserData;
		check(OverrideColorVertexBuffer);

		if (!LocalVertexFactory->SupportsManualVertexFetch(FeatureLevel))
		{
			LocalVertexFactory->GetColorOverrideStream(OverrideColorVertexBuffer, VertexStreams);
		}	
	}
	
	const FInstancingUserData* InstancingUserData = (const FInstancingUserData*)BatchElement.UserData;
	const auto* InstancedVertexFactory = static_cast<const FVisMeshInstancedVertexFactory*>(VertexFactory);
	const int32 InstanceOffsetValue = BatchElement.UserIndex;

	ShaderBindings.Add(InstanceOffset, InstanceOffsetValue);
	
	//if (!UseGPUScene(Scene ? Scene->GetShaderPlatform() : GMaxRHIShaderPlatform))
	{
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FInstancedVisMeshVertexFactoryUniformShaderParameters>(), InstancedVertexFactory->GetInstanceUniformBuffer());
		if (InstancedVertexFactory->SupportsManualVertexFetch(FeatureLevel))
		{
			ShaderBindings.Add(VertexFetch_InstanceOriginBufferParameter, InstancedVertexFactory->GetInstanceOriginSRV());
			ShaderBindings.Add(VertexFetch_InstanceTransformBufferParameter, InstancedVertexFactory->GetInstanceTransformSRV());
			ShaderBindings.Add(VertexFetch_InstanceLightmapBufferParameter, InstancedVertexFactory->GetInstanceLightmapSRV());
		}
		if (InstanceOffsetValue > 0 && VertexStreams.Num() > 0)
		{
			// GPUCULL_TODO: This here can still work together with the instance attributes for index, but note that all instance attributes then must assume they are offset wrt the on-the-fly generate buffer
			//          so with the new scheme there is no clear way this can work in the vanilla instancing way as there is an indirection. So either other attributes must be loaded in the shader or they
			//          would have to be copied as the instance ID is now - not good.
			VertexFactory->OffsetInstanceStreams(InstanceOffsetValue, InputStreamType, VertexStreams);
		}
	}

	FVector4f InstancingOffset(ForceInit);
	// InstancedLODRange is only set for HierarchicalInstancedStaticMeshes
	if (InstancingUserData && BatchElement.InstancedLODRange)
	{
		InstancingOffset = (FVector3f)InstancingUserData->InstancingOffset; // LWC_TODO: precision loss
	}
	ShaderBindings.Add(InstancingOffsetParameter, InstancingOffset);

	ShaderBindings.Add(Shader->GetUniformBufferParameter<FInstancedVisMeshVFLooseUniformShaderParameters>(), InstancedVertexFactory->GetInstanceLooseUniformBuffer());
}
