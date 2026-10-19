#include "UAKAbilityTask_PlayMontage.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityContext.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimMontage.h"

#define LOCTEXT_NAMESPACE "UAKAbilityTask_PlayMontage"

UAKAbilityTask_PlayMontage::UAKAbilityTask_PlayMontage()
{
	// 表现型任务：默认只在本地端播放
	NetMode = EAKTaskNetMode::ClientOnly;
}

void UAKAbilityTask_PlayMontage::OnTaskStart(FAKAbilityContext& Context)
{
	UAbilitySystemComponent* ASC = Context.SourceASC.Get();
	UGameplayAbility* Ability = Context.Ability.Get();
	if (!ASC || !Ability || !Montage)
	{
		return;
	}

	ASC->PlayMontage(Ability, Ability->GetCurrentActivationInfo(), Montage, Rate, StartSection);
}

void UAKAbilityTask_PlayMontage::OnTaskEnd(FAKAbilityContext& Context, bool bInterrupted)
{
	if (!bStopOnEnd)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = Context.SourceASC.Get())
	{
		ASC->CurrentMontageStop();
	}
}

void UAKAbilityTask_PlayMontage::Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const
{
	Super::Validate(Asset, OutResult);

	if (!Montage)
	{
		OutResult.Add(EAKValidationSeverity::Error,
			LOCTEXT("NoMontage", "PlayMontage 任务未指定 Montage。"),
			const_cast<UAKAbilityTask_PlayMontage*>(this));
	}
}

void UAKAbilityTask_PlayMontage::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);

	OutJson->SetStringField(TEXT("montage"), Montage ? Montage->GetPathName() : TEXT(""));
	OutJson->SetNumberField(TEXT("rate"), Rate);
	OutJson->SetStringField(TEXT("startSection"), StartSection.ToString());
	OutJson->SetBoolField(TEXT("stopOnEnd"), bStopOnEnd);
}

void UAKAbilityTask_PlayMontage::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	FString MontagePath;
	if (InJson.TryGetStringField(TEXT("montage"), MontagePath) && !MontagePath.IsEmpty())
	{
		Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	}

	double OutRate = Rate;
	if (InJson.TryGetNumberField(TEXT("rate"), OutRate))
	{
		Rate = static_cast<float>(OutRate);
	}

	FString Section;
	if (InJson.TryGetStringField(TEXT("startSection"), Section))
	{
		StartSection = FName(*Section);
	}

	InJson.TryGetBoolField(TEXT("stopOnEnd"), bStopOnEnd);
}

#undef LOCTEXT_NAMESPACE
