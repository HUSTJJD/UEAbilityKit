#pragma once

#include "AssetTypeActions_Base.h"
#include "Toolkits/IToolkitHost.h"

/** UAKAbilityAsset 在内容浏览器中的类型动作（创建、颜色、分类、双击打开自定义编辑器） */
class FUAKAbilityAssetTypeActions : public FAssetTypeActions_Base
{
public:
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;

	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects,
		TSharedPtr<IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
};
