// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshWeaverCore.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FMeshWeaverCoreModule"

void FMeshWeaverCoreModule::StartupModule()
{
	const FString VirtualShaderPath = TEXT("/MeshWeaverCoreShaders");
	if (!AllShaderSourceDirectoryMappings().Contains(VirtualShaderPath))
	{
		const FString PluginShaderDir = FPaths::Combine(
			IPluginManager::Get().FindPlugin(TEXT("DelaunatorPlugin"))->GetBaseDir(),
			TEXT("Shaders/Private"));
		AddShaderSourceDirectoryMapping(VirtualShaderPath, PluginShaderDir);
	}
}

void FMeshWeaverCoreModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMeshWeaverCoreModule, MeshWeaverCore)
