#include "UAKAbilityAssetFactory.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTrack.h"
#include "UAKTypes.h"

UAKAbilityAssetFactory::UAKAbilityAssetFactory()
{
	SupportedClass = UAKAbilityAsset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UAKAbilityAssetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName,
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UAKAbilityAsset* Asset = NewObject<UAKAbilityAsset>(InParent, InClass, InName, Flags);
	if (!Asset)
	{
		return nullptr;
	}

	// 默认给一条 Main 轨道，避免新资产是一片空白
	UAKAbilityTrack* MainTrack = NewObject<UAKAbilityTrack>(Asset);
	MainTrack->TrackName = FName(TEXT("Main"));
	MainTrack->TrackType = EAKTrackType::Main;
	Asset->Tracks.Add(MainTrack);

	Asset->DisplayName = FText::FromName(InName);
	Asset->Length = 1.f;
	Asset->MarkPackageDirty();

	return Asset;
}
