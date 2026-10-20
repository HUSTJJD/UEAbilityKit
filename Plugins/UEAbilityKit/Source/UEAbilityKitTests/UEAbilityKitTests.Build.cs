using UnrealBuildTool;

public class UEAbilityKitTests : ModuleRules
{
	public UEAbilityKitTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

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
			"Json",
			"JsonUtilities",
			"Slate",
			"SlateCore",
			"UnrealEd",          // 编辑器上下文自动化（FAutomationEditorCommonUtils 等）
			"AssetRegistry"
		});
	}
}
