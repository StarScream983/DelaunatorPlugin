// Copyright Epic Games, Inc. All Rights Reserved.

#include "DelaunatorPlugin.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FDelaunatorPluginModule"

void FDelaunatorPluginModule::StartupModule()
{
	// Map virtual shader path used by IMPLEMENT_GLOBAL_SHADER / IMPLEMENT_VERTEX_FACTORY_TYPE
	// e.g. "/IndirectInstancingShaders/GeoVoronoiIndirectInstancing/GeoVoronoiIndirectInstancingCompute.usf"
	//   -> "<PluginDir>/Shaders/Private/GeoVoronoiIndirectInstancing/GeoVoronoiIndirectInstancingCompute.usf"
	FString VirtualShaderPath = TEXT("/IndirectInstancingShaders");
	if (!AllShaderSourceDirectoryMappings().Contains(VirtualShaderPath))
	{
		FString PluginShaderDir = FPaths::Combine(
			IPluginManager::Get().FindPlugin(TEXT("DelaunatorPlugin"))->GetBaseDir(),
			TEXT("Shaders/Private"));
		AddShaderSourceDirectoryMapping(VirtualShaderPath, PluginShaderDir);
	}
}

void FDelaunatorPluginModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDelaunatorPluginModule, DelaunatorPlugin)