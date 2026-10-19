#include "UAKAbilityAssetTypeActions.h"

#include "UEAbilityKitAssetEditor.h"
#include "UAKAbilityAsset.h"

#define LOCTEXT_NAMESPACE "UAKAbilityAssetTypeActions"

FText FUAKAbilityAssetTypeActions::GetName() const
{
	return LOCTEXT("AssetTypeActions_AbilityAsset_Name", "Ability Asset");
}

UClass* FUAKAbilityAssetTypeActions::GetSupportedClass() const
{
	return UAKAbilityAsset::StaticClass();
}

FColor FUAKAbilityAssetTypeActions::GetTypeColor() const
{
	return FColor(97, 218, 251);
}

void FUAKAbilityAssetTypeActions::OpenAssetEditor(const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	// 打开我们自己的三视图编辑器，而不是默认的属性面板
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid()
		? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (UObject* Object : InObjects)
	{
		if (UAKAbilityAsset* Asset = Cast<UAKAbilityAsset>(Object))
		{
			TSharedRef<FUEAbilityKitAssetEditor> Editor = MakeShared<FUEAbilityKitAssetEditor>();
			Editor->InitEditor(Mode, EditWithinLevelEditor, Asset);
		}
	}
}

#undef LOCTEXT_NAMESPACE
