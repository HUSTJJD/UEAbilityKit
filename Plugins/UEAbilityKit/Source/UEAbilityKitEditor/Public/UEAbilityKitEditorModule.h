#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"

class IAssetTypeActions;

class FUEAbilityKitEditorModule : public IModuleInterface
{
public:
	/** 调试面板的 Nomad Tab 名（也用于 FGlobalTabmanager::TryInvokeTab） */
	static const FName DebuggerTabName;

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** 已注册的资产类型动作，卸载时需要反注册 */
	TArray<TSharedPtr<IAssetTypeActions>> RegisteredAssetTypeActions;

	TSharedRef<SDockTab> SpawnDebuggerTab(const FSpawnTabArgs& Args);
};
