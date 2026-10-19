#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class IAssetTypeActions;

class FUEAbilityKitEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** 已注册的资产类型动作，卸载时需要反注册 */
	TArray<TSharedPtr<IAssetTypeActions>> RegisteredAssetTypeActions;
};
