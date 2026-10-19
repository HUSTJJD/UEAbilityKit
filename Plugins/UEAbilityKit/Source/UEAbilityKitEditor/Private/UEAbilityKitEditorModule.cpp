#include "UEAbilityKitEditorModule.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityAssetTypeActions.h"
#include "UAKAbilityThumbnailRenderer.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ThumbnailRendering/ThumbnailManager.h"

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

	if (UObjectInitialized() && !IsEngineExitRequested())
	{
		UThumbnailManager::Get().UnregisterCustomRenderer(UAKAbilityAsset::StaticClass());
	}
}

IMPLEMENT_MODULE(FUEAbilityKitEditorModule, UEAbilityKitEditor)
