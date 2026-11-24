using UnrealBuildTool;

// 自動テスト（CQTest）。出荷物には含めない（DeveloperTool）。テスト名は Tennis.<モジュール>.<分類>（ARCH-03 T-2）。
public class TennisTests : ModuleRules
{
	public TennisTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// テストは各ファイルの無名の名前空間に似た名前の補助（Player・Dt など）を持つので、unity build でまとめない。
		// まとめると、CI のきれいなビルドだけで名前がぶつかる（手元は変更中のファイルがまとめから外れるため気づけない）
		bUseUnity = false;

		PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "AssetRegistry", "CQTest", "TennisSim", "Tennis", "EnhancedInput", "InputCore", "Chooser", "HairStrandsCore" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new[] { "TennisEditor", "PoseSearch" });  // Chooser の表・ロコモーションのデータベースを作る道具・焼く道具（エディタ専用）のテスト
		}
	}
}
