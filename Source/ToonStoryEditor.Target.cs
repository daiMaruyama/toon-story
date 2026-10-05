// Copyright (c) 2026 Mamodou. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ToonStoryEditorTarget : TargetRules
{
	public ToonStoryEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("ToonStory");
	}
}
