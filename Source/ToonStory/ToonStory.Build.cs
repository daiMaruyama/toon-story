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
			"RenderCore",
			"AnimationCore",
			"AIModule",
			"NavigationSystem",
			"DeveloperSettings",
			"InputCore",
			"CoreOnline",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"SignalProcessing",
			"AudioMixer",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PublicIncludePaths.AddRange(new string[] {
			"ToonStory"
		});
		// チャットの伏せ字にSteamのFilterTextを直接呼ぶ。
		bool bSteam = Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Win64 ||
		              Target.Platform == UnrealTargetPlatform.Linux;
		if (bSteam)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
		}
		PrivateDefinitions.Add("WITH_TB_STEAM=" + (bSteam ? "1" : "0"));
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
