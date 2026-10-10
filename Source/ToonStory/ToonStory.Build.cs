// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ToonStory : ModuleRules
{
	public ToonStory(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"EnhancedInput",
			"UMG",
			"Slate"
		});

		PublicIncludePaths.AddRange(new string[] {
			"ToonStory"
		});
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "AssetRegistry", "ImageCore", "RenderCore", "RHI" });
		}
		// Keep downloaded toy attribution alongside packaged builds when this optional Content is present.
		if (Target.ProjectFile != null)
		{
			string ToyNotices = System.IO.Path.Combine(Target.ProjectFile.Directory.FullName,
				"Content", "ThirdPartyNotices", "ToyAssets.txt");
			if (System.IO.File.Exists(ToyNotices))
			{
				RuntimeDependencies.Add(ToyNotices, StagedFileType.NonUFS);
			}
		}

	}
}
