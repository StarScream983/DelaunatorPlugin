// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class IndirectInstancingCore : ModuleRules
{
	public IndirectInstancingCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
				// ... add public include paths required here ...
				Path.Combine(ModuleDirectory, "Private"),
            }
            );
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
				Path.Combine(ModuleDirectory, "Private"),
            }
			);

        // Tell the compiler we want to import the ImPlot symbols when linking against ImGui plugin 
        PrivateDefinitions.Add(string.Format("IMPLOT_API=DLLIMPORT"));

        

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"DelaunatorPlugin",
			"Large_CBT",
            "RenderCore",
			"RHI",
			"Renderer",
			"Engine",
			"Projects",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
		});
	}
}
