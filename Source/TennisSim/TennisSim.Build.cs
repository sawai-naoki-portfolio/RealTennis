using UnrealBuildTool;

// 純粋なシミュレーション（ボール物理・ソルバ・ルール・スコア）。Engine に依存しない（ARCH-02 §1・§2）。
public class TennisSim : ModuleRules
{
	public TennisSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Engine に依存しない（FVector・USTRUCT は Core / CoreUObject にある）。
		// 浮動小数点の最適化：決定論のため /fp:fast を使わない（既定の /fp:precise のまま）
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject" });
	}
}
