using UnrealBuildTool;

public class TennisEditorTarget : TargetRules
{
	public TennisEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new[] { "Tennis", "TennisTests", "TennisEditor" });
	}
}
