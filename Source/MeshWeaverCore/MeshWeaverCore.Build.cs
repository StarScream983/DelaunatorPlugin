// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MeshWeaverCore : ModuleRules
{
	public MeshWeaverCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModuleDirectory, "Private"),
			}
			);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Large_CBT",
				"RenderCore",
				"Renderer",
				"RHI",
				"Engine",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Projects",
			}
			);
	}
}
