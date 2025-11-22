// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisMesh.h"

#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FVisMeshModule"

void FVisMeshModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	const FString ShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("VisMesh"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/VisMeshPlugin"), ShaderDir);
}

void FVisMeshModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FVisMeshModule, VisMesh)