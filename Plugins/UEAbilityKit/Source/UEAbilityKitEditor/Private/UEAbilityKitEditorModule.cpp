#include "UEAbilityKitEditorModule.h"

#include "UAKAbilityAssetTypeActions.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"

void FUEAbilityKitEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// 注册资产类型：内容浏览器右键 → Miscellaneous → Ability Asset
	RegisteredAssetTypeActions.Add(MakeShared<FUAKAbilityAssetTypeActions>());
	for (const TSharedPtr<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
	{
		AssetTools.RegisterAssetTypeActions(Action.ToSharedRef());
	}

	// TODO(M2): 注册 FAssetEditorToolkit（时间轴编辑器：轨道 / 视口 / 详情 / 工具栏）。
}

void FUEAbilityKitEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (const TSharedPtr<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
		}
	}
	RegisteredAssetTypeActions.Empty();
}

IMPLEMENT_MODULE(FUEAbilityKitEditorModule, UEAbilityKitEditor)
