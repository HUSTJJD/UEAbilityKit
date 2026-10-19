using UnrealBuildTool;

public class UAKDemo : ModuleRules
{
	public UAKDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"UEAbilityKit",

			// 脚本方案（git submodule：Plugins/UnLuaNova）
			// 仅示例模块依赖它；UAK 插件本身不依赖任何脚本系统
			"UnLuaNova"
		});
	}
}
