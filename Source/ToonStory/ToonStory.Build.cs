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
			"SignalProcessing",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PublicIncludePaths.AddRange(new string[] {
			"ToonStory"
		});
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
