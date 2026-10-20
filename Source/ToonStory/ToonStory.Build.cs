// Copyright (c) 2026 Mamodou. All Rights Reserved.

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
			"AnimationCore",
			"AIModule",
			"NavigationSystem",
			"DeveloperSettings",
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
			foreach (string Relative in new[] { "UI/PaperClub/Fonts", "ThirdPartyNotices/PaperClub" })
			{
				string Directory = System.IO.Path.Combine(Target.ProjectFile.Directory.FullName, "Content", Relative);
				string Pattern = Relative.EndsWith("Fonts") ? "*.ttf" : "*.txt";
				if (System.IO.Directory.Exists(Directory))
				{
					foreach (string File in System.IO.Directory.GetFiles(Directory, Pattern))
					{
						RuntimeDependencies.Add(File, StagedFileType.NonUFS);
					}
				}
			}

			string ToyNotices = System.IO.Path.Combine(Target.ProjectFile.Directory.FullName,
				"Content", "ThirdPartyNotices", "ToyAssets.txt");
			if (System.IO.File.Exists(ToyNotices))
			{
				RuntimeDependencies.Add(ToyNotices, StagedFileType.NonUFS);
			}
		}

	}
}
