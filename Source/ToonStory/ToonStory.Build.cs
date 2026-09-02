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
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ToonStory",
			"ToonStory/Variant_Platforming",
			"ToonStory/Variant_Platforming/Animation",
			"ToonStory/Variant_Combat",
			"ToonStory/Variant_Combat/AI",
			"ToonStory/Variant_Combat/Animation",
			"ToonStory/Variant_Combat/Gameplay",
			"ToonStory/Variant_Combat/Interfaces",
			"ToonStory/Variant_Combat/UI",
			"ToonStory/Variant_SideScrolling",
			"ToonStory/Variant_SideScrolling/AI",
			"ToonStory/Variant_SideScrolling/Gameplay",
			"ToonStory/Variant_SideScrolling/Interfaces",
			"ToonStory/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
