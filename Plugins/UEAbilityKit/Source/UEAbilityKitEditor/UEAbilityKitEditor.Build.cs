using UnrealBuildTool;

public class UEAbilityKitEditor : ModuleRules
{
	public UEAbilityKitEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UEAbilityKit"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"AssetTools",
			"Slate",
			"SlateCore",
			"EditorStyle",
			"PropertyEditor",
			"WorkspaceMenuStructure",
			"InputCore",
			"AssetRegistry",
			"Json",
			"JsonUtilities"
		});
	}
}
