using UnrealBuildTool;

public class LyraAdapt : ModuleRules
{
	public LyraAdapt(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",

			// 适配层同时认识两边：这是它存在的唯一理由
			"LyraGame",      // 宿主（拷进来的 Lyra）
			"UEAbilityKit"   // 产品插件（技能模块）
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EnhancedInput"   // Lyra 的输入是 Enhanced Input，绑定按住状态要用
		});
	}
}
