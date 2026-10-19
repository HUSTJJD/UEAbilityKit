using UnrealBuildTool;
using System.Collections.Generic;

public class UAKDemoEditorTarget : TargetRules
{
	public UAKDemoEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		ExtraModuleNames.Add("UAKDemo");
	}
}
