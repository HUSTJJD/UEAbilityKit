#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "UAKAbilityAssetFactory.generated.h"

/** 内容浏览器里创建 UAKAbilityAsset 的工厂（右键 → Miscellaneous → Ability Asset） */
UCLASS()
class UEABILITYKITEDITOR_API UAKAbilityAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UAKAbilityAssetFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

	virtual bool CanCreateNew() const override { return true; }
};
