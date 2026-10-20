using UnrealBuildTool;
using System.Collections.Generic;

public class UAKDemoEditorTarget : TargetRules
{
	public UAKDemoEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		// 跟随引擎自身默认值，而不是写死版本号：Editor 目标与 UnrealEditor 共用构建环境，
		// 一旦目标的编译告警设置与它不一致，UBT 会直接拒绝构建（UEBuildTarget.ValidateSharedEnvironment）。
		// 写死 V6 之类的值在引擎升到 V7 时会立刻失效。
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("UAKDemo");
	}
}
