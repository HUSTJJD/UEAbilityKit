using UnrealBuildTool;
using System.Collections.Generic;

public class UAKDemoServerTarget : TargetRules
{
	public UAKDemoServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("UAKDemo");
	}
}
