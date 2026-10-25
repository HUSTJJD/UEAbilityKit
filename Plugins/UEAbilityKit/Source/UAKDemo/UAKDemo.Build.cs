using UnrealBuildTool;

public class UAKDemo : ModuleRules
{
	public UAKDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// 这里**故意不列 UnLuaNova**。
		//
		// 示例的 Lua 绑定走 UnLuaNova 的数据驱动路线：Config/DefaultUnLuaNova.ini
		// 里把 AKDemoCharacter 映射到 Content/Script/UAKDemoCharacter.lua
		// （见 ULuaModuleLocator_ByBinding）。不再由 AAKDemoCharacter 实现
		// IUnLuaNovaInterface，于是示例模块对脚本方案**零依赖**：
		// 把 Plugins/UnLuaNova 整个删掉，本模块照样编译、照样跑。
		//
		// 顺带也躲开了一个坑：以前这里还得"再列一次 Lua 后端模块"
		// （UBT 的依赖不递归传递链接项，少列就 LNK2019）。现在没有脚本依赖，也就没有后端。
		// 真正要用 UnLuaNova 写 C++ 扩展的模块，照着 docs/examples/04 做，并跑
		// Tools/unluanova.ps1 doctor 自检。
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"UEAbilityKit"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects"        // IPluginManager（示例资产写到插件自己的 Content 目录）
		});

		// UAKDemoSetupCommandlet：一键生成示例资产 / 蓝图 / 关卡。
		// 只在编辑器目标里编译（WITH_EDITOR），不进打包产物。
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",       // FEditorFileUtils（关卡创建与保存）
				"Kismet",         // FKismetEditorUtilities（建蓝图）
				"AssetRegistry",  // FAssetRegistryModule::AssetCreated
				"Json",
				"JsonUtilities"
			});
		}
	}
}
