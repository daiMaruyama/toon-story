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

	}
}
