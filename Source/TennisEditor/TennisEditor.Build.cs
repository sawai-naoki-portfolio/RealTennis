using UnrealBuildTool;

// エディタ専用のツール（ARCH-02 §2.3）。出荷物には含めない。依存は、ツールを足すときに必要な分だけ足す（YAGNI）。
public class TennisEditor : ModuleRules
{
	public TennisEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "AssetRegistry", "Chooser", "AnimationBlueprintLibrary", "PoseSearch", "Tennis", "TennisSim" });  // PoseSearch：ロコモーションのデータベースを作る道具（DD-03 §6.5）
	}
}
