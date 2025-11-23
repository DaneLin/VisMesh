#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderPermutation.h"
#include "ShaderParameterStruct.h"
#include "ShaderParameterUtils.h"

class FPopulateVertexAndIndirectBufferCS : public FGlobalShader
{
	SHADER_USE_PARAMETER_STRUCT(FPopulateVertexAndIndirectBufferCS, FGlobalShader);
	DECLARE_EXPORTED_GLOBAL_SHADER(FPopulateVertexAndIndirectBufferCS, VISMESH_API);

public:
	static constexpr uint32 ThreadGroupSize = 256;
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, VISMESH_API)
		SHADER_PARAMETER_UAV(RWBuffer<float>, OutInstanceVertices)
		SHADER_PARAMETER_UAV(RWBuffer<uint>, OutIndirectArgs)

		SHADER_PARAMETER(float, XSpace)
		SHADER_PARAMETER(float, YSpace)
		SHADER_PARAMETER(int, NumColumns)
		SHADER_PARAMETER(int, NumInstances)
		SHADER_PARAMETER(float, Time)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

class FPopulateBoxChartInstanceBufferCS : public FGlobalShader
{
	SHADER_USE_PARAMETER_STRUCT(FPopulateBoxChartInstanceBufferCS, FGlobalShader);
	DECLARE_EXPORTED_GLOBAL_SHADER(FPopulateBoxChartInstanceBufferCS, VISMESH_API);

public:
	static constexpr uint32 ThreadGroupSize = 256;
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, VISMESH_API)
		SHADER_PARAMETER_UAV(RWBuffer<float4>, OutInstanceOriginBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float4>, OutInstanceTransforms)
		SHADER_PARAMETER_UAV(RWBuffer<uint>, OutIndirectArgs)

		SHADER_PARAMETER(float, XSpace)
		SHADER_PARAMETER(float, YSpace)
		SHADER_PARAMETER(int, NumColumns)
		SHADER_PARAMETER(int, NumInstances)
		SHADER_PARAMETER(float, Time)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

class FPopulateBoxWireframeBufferCS : public FGlobalShader
{
	SHADER_USE_PARAMETER_STRUCT(FPopulateBoxWireframeBufferCS, FGlobalShader);
	DECLARE_EXPORTED_GLOBAL_SHADER(FPopulateBoxWireframeBufferCS, VISMESH_API);

public:
	static constexpr uint32 ThreadGroupSize = 256;
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, VISMESH_API)
		SHADER_PARAMETER_UAV(RWBuffer<float>, OutInstanceVertices)
		SHADER_PARAMETER_UAV(RWBuffer<uint>, OutIndirectArgs)

		SHADER_PARAMETER(float, XSpace)
		SHADER_PARAMETER(float, YSpace)
		SHADER_PARAMETER(int, NumColumns)
		SHADER_PARAMETER(int, NumInstances)
		SHADER_PARAMETER(float, LineWidth)
		SHADER_PARAMETER(FVector4f, CameraPos)
		SHADER_PARAMETER(float, Time)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};
