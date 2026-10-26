using UnrealBuildTool;
using System.IO;
using System.Linq;

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
			"JsonUtilities",
			// 预览默认值：读 /Script/EngineSettings.GameMapsSettings 的 GlobalDefaultGameMode，
			// 好让预览角色不必在每个资产上手填。
			"EngineSettings",
			// 教程自动化：FWidgetRenderer 把面板渲成 PNG（UMG / RenderCore / ImageWrapper）
			"UMG",
			"RenderCore",
			"ImageWrapper"
		});

		// ── 交互引导（UAK.Tutorial.Guided）────────────────────────────────
		// 复用引擎自带的 IntroTutorials 做"读者动手"那一半。
		//
		// 为什么要探测而不是直接写死依赖：这个模块的位置一直在变
		// （UE4/5.4 在 Engine/Source/Editor/IntroTutorials，5.8 挪进了
		// Engine/Plugins/Editor/GuidedTutorials，而且默认不启用）。
		// 探测不到就不编进来，桥接层整体降级为"不可用"，UAK 本体照常工作。
		string[] IntroCandidates =
		{
			Path.Combine(EngineDirectory, "Plugins", "Editor", "GuidedTutorials", "Source", "IntroTutorials"),
			Path.Combine(EngineDirectory, "Source", "Editor", "IntroTutorials"),
		};

		bool bHasIntroTutorials = IntroCandidates.Any(Directory.Exists);
		if (bHasIntroTutorials)
		{
			PrivateDependencyModuleNames.Add("IntroTutorials");
		}
		PublicDefinitions.Add(bHasIntroTutorials
			? "UAK_WITH_INTRO_TUTORIALS=1"
			: "UAK_WITH_INTRO_TUTORIALS=0");
	}
}
