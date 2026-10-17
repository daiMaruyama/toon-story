using UnrealBuildTool;
public class ToonStoryEditor : ModuleRules
{
 public ToonStoryEditor(ReadOnlyTargetRules Target) : base(Target)
 {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PrivateDependencyModuleNames.AddRange(new[] {
   "Core", "CoreUObject", "Engine", "UnrealEd", "ToolMenus", "ContentBrowser", "Slate", "SlateCore", "ToonStory",
   "AssetRegistry"
  });
 }
}
