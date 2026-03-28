// Copyright Epic Games, Inc. All Rights Reserved.


#include "IndirectInstancingCore.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FIndirectInstancingCoreModule"

	void FIndirectInstancingCoreModule::StartupModule()
	{
		FString PluginShaderDir = FPaths::Combine(
			IPluginManager::Get().FindPlugin(TEXT("DelaunatorPlugin"))->GetBaseDir(),
			TEXT("Shaders/Private"));

		if (!AllShaderSourceDirectoryMappings().Contains(TEXT("/IndirectInstancingCoreShaders")))
		{
			AddShaderSourceDirectoryMapping(TEXT("/IndirectInstancingCoreShaders"), PluginShaderDir);
		}
	}

	void FIndirectInstancingCoreModule::ShutdownModule()
	{
		// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
		// we call this function before unloading the module.
	}

IMPLEMENT_MODULE(FIndirectInstancingCoreModule, IndirectInstancingCore)