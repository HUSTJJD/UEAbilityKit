using UnrealBuildTool;
using System.Collections.Generic;

public class UAKDemoServerTarget : TargetRules
{
	public UAKDemoServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		ExtraModuleNames.Add("UAKDemo");
	}
}
