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

	// UE 5.8 的 FAssetTypeActions_Base 不再提供默认实现，子类必须自己给分类。
	// 归到 Gameplay：技能是 GAS 领域的资产，不该躺在 Miscellaneous 里让人翻不到。
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Gameplay; }

	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects,
		TSharedPtr<IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
};
