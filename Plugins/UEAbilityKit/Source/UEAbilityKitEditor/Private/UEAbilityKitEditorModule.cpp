#include "UEAbilityKitEditorModule.h"

#include "SUAKDebuggerPanel.h"
#include "UAKAbilityAsset.h"
#include "UAKAbilityAssetTypeActions.h"
#include "UAKAbilityThumbnailRenderer.h"
#include "UAKTutorialAutomation.h"
#include "UAKTutorialGuided.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Framework/Docking/TabManager.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FUEAbilityKitEditorModule"

const FName FUEAbilityKitEditorModule::DebuggerTabName = TEXT("UAKDebugger");

void FUEAbilityKitEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// 注册资产类型：内容浏览器右键 → Miscellaneous → Ability Asset
	RegisteredAssetTypeActions.Add(MakeShared<FUAKAbilityAssetTypeActions>());
	for (const TSharedPtr<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
	{
		AssetTools.RegisterAssetTypeActions(Action.ToSharedRef());
	}

	// 技能资产缩略图：画"技能结构示意图"
	UThumbnailManager::Get().RegisterCustomRenderer(
		UAKAbilityAsset::StaticClass(), UAKAbilityThumbnailRenderer::StaticClass());

	// 调试面板：窗口 → 开发者工具 → 调试 → UAK Debugger
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(DebuggerTabName,
		FOnSpawnTab::CreateRaw(this, &FUEAbilityKitEditorModule::SpawnDebuggerTab))
		.SetDisplayName(LOCTEXT("DebuggerTabTitle", "UAK Debugger"))
		.SetTooltipText(LOCTEXT("DebuggerTabTooltip", "实时列出 PIE 中正在运行的 UAK 技能与任务"))
		.SetMenuType(ETabSpawnerMenuType::Enabled)
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());

	// 教程自动化：UAK.Tutorial.*（录制"编辑器的使用过程"并产出文档素材）
	FUAKTutorialAutomation::Register();

	// 交互引导：同一份教程脚本，换成"读者自己动手、引擎判定进度"的形态。
	// 引擎没带 IntroTutorials 时这里只是注册一条会明确报错的命令，不影响其它功能。
	FUAKTutorialGuided::Register();
}

void FUEAbilityKitEditorModule::ShutdownModule()
{
	FUAKTutorialRunner::Get().Abort();
	FUAKTutorialGuided::Unregister();
	FUAKTutorialAutomation::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DebuggerTabName);

	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (const TSharedPtr<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
		}
	}
	RegisteredAssetTypeActions.Empty();

	if (UObjectInitialized() && !IsEngineExitRequested())
	{
		UThumbnailManager::Get().UnregisterCustomRenderer(UAKAbilityAsset::StaticClass());
	}
}

TSharedRef<SDockTab> FUEAbilityKitEditorModule::SpawnDebuggerTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SUAKDebuggerPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUEAbilityKitEditorModule, UEAbilityKitEditor)
