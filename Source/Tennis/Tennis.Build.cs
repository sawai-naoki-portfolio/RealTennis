using UnrealBuildTool;

// ゲーム本体（Actor・Component・GameMode・AI・入力・演出・UI）。依存はマイルストーンで必要になったときに足す（YAGNI）。
public class Tennis : ModuleRules
{
	public Tennis(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "DeveloperSettings", "TennisSim" });
		PrivateDependencyModuleNames.AddRange(new[] { "InputCore", "EnhancedInput", "Chooser", "HairStrandsCore" });  // 打球ツールのキー（EKeys）、選手の操作（Enhanced Input）、ストロークの選択（Chooser）、選手の髪（Groom：DD-03 §7）
	}
}
