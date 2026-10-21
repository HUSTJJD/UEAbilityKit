using System;
using System.IO;
#if UE_5_0_OR_LATER
using EpicGames.Core;
#else
using Tools.DotNETCommon;
#endif
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

		// Lua 后端必须在这里再列一次，哪怕 UnLuaNova 自己已经依赖它。
		//
		// UnLuaNova 的公共头文件直接调用 lua_*（LuaBackend.h 以及基于它的模板
		// FLuaFunction::Call / TConstructor<>::Register 等）——这些模板是在**本模块**
		// 里实例化的，所以对 lua_gettop 等符号的引用出现在 UAKDemo 自己的目标文件里。
		// 而 UEBuildModule.GetDependencies() 不是递归的：一个模块的依赖不会进入
		// 它消费者的链接命令行，只有 include 路径会传递。
		// 结果就是 UnrealEditor-Luau.lib 只在 UnLuaNova 的链接行上，不在 UAKDemo 的，
		// 于是 lua_* 全变成未解析符号（LNK2019，或运行期踩到错误实现而崩溃）。
		//
		// 用哪个后端是 UnLuaNova 的编译期选择，这里读同一个配置项，保证两边不会不一致。
		// 不能把 Lua 和 Luau 同时列进来：两者都提供同名 lua.h / lualib.h，
		// 会同时进 include 路径，并触发 LuaBackend.h 的"一个构建里出现两个后端"检查。
		// 配置文件不存在时 ConfigFile 为空，GetString 返回 false，走默认 Luau。
		var configFile = new FileReference(
			Path.Combine(Target.ProjectFile.Directory.FullName, "Config", "DefaultUnLuaNovaEditor.ini"));
		var config = new ConfigHierarchy(new[]
		{
			FileReference.Exists(configFile) ? new ConfigFile(configFile) : new ConfigFile()
		});
		const string section = "/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings";

		string backend;
		if (!config.GetString(section, "LuaBackend", out backend))
		{
			backend = "Luau";
		}

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			string.Equals(backend, "Lua", StringComparison.OrdinalIgnoreCase) ? "Lua" : "Luau"
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
