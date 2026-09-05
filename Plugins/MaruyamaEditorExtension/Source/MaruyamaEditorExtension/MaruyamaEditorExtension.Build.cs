using UnrealBuildTool;

public class MaruyamaEditorExtension : ModuleRules
{
	public MaruyamaEditorExtension(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UnrealEd",
			"DeveloperSettings",
			"Settings",

			// Epic公式MCPプラグイン。サーバー制御とクライアント設定生成に使う。
			"ModelContextProtocol",
			"ModelContextProtocolEngine"
		});
	}
}
