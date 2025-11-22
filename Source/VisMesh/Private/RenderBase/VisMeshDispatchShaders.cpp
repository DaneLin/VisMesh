#include "RenderBase/VisMeshDispatchShaders.h"

#include "ShaderParameterUtils.h"

IMPLEMENT_GLOBAL_SHADER(FPopulateVertexAndIndirectBufferCS, "/VisMeshPlugin/PopulateVertexAndIndirectBuffer.usf", "MainCS",SF_Compute);

void FPopulateVertexAndIndirectBufferCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_COUNT"), ThreadGroupSize);
}