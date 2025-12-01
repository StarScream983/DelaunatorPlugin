// Copyright Epic Games, Inc. All Rights Reserved.

#include "Large_CBT.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FLarge_CBTModule"

void FLarge_CBTModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
    const FString PluginShaderDir =
        FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("DelaunatorPlugin"))->GetBaseDir(),
            TEXT("Shaders/Private"));

    // Map /MyShaders to your plugin's Shaders folder
    AddShaderSourceDirectoryMapping(TEXT("/DelaunatorPlugin"), PluginShaderDir);

    /*UE_LOG(LogTemp, Warning, TEXT("ShaderDir mapped: %s"), *PluginShaderDir);
    checkf(FPaths::FileExists(PluginShaderDir / TEXT("RaymarchPS.usf")),
        TEXT("RaymarchPS.usf not found at %s"), *(PluginShaderDir / TEXT("RaymarchPS.usf")));*/
}

void FLarge_CBTModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLarge_CBTModule, Large_CBT)