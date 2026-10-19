using UnrealBuildTool;
using System.Collections.Generic;

public class UAKDemoTarget : TargetRules
{
	public UAKDemoTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		ExtraModuleNames.Add("UAKDemo");
	}
}
