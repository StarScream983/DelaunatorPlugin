// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DelaunatorPlugin : ModuleRules
{
	public DelaunatorPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
            }
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);

        // Tell the compiler we want to import the ImPlot symbols when linking against ImGui plugin 
        PrivateDefinitions.Add(string.Format("IMPLOT_API=DLLIMPORT"));

        PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
				"Large_CBT",
				"SleefPlugin",
                "SLEEF",
				"ImGui"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
				"Projects",
            }
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
