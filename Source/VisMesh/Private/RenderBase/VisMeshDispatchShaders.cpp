#include "RenderBase/VisMeshDispatchShaders.h"

#include "ShaderParameterUtils.h"

IMPLEMENT_GLOBAL_SHADER(FPopulateVertexAndIndirectBufferCS, "/VisMeshPlugin/DispatchShaders/PopulateVertexAndIndirectBuffer.usf", "MainCS",SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FPopulateBoxChartInstanceBufferCS, "/VisMeshPlugin/DispatchShaders/PopulateBoxChartInstanceBuffer.usf", "MainCS",SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FPopulateBoxChartFrustumCulledInstanceBufferCS, "/VisMeshPlugin/DispatchShaders/PopulateBoxChartFrustumCulledInstanceBuffer.usf", "MainCS",SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FPopulateBoxWireframeBufferCS, "/VisMeshPlugin/DispatchShaders/PopulateBoxWireframeBuffer.usf", "MainCS",SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FPopulateBoxWireframeMiterBufferCS, "/VisMeshPlugin/DispatchShaders/PopulateBoxWireframeBuffer_Miter.usf", "MainCS",SF_Compute);

void FPopulateVertexAndIndirectBufferCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_COUNT"), ThreadGroupSize);
}

void FPopulateBoxChartInstanceBufferCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_COUNT"), ThreadGroupSize);
}

void FPopulateBoxChartFrustumCulledInstanceBufferCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_COUNT"), ThreadGroupSize);
}

void FPopulateBoxWireframeBufferCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_COUNT"), ThreadGroupSize);
}

void FPopulateBoxWireframeMiterBufferCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_COUNT"), ThreadGroupSize);
}
