#include "UAKAbilityTask_SendGameplayCue.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityContext.h"

#include "AbilitySystemComponent.h"

#define LOCTEXT_NAMESPACE "UAKAbilityTask_SendGameplayCue"

UAKAbilityTask_SendGameplayCue::UAKAbilityTask_SendGameplayCue()
{
	NetMode = EAKTaskNetMode::ClientOnly;
}

void UAKAbilityTask_SendGameplayCue::OnTaskStart(FAKAbilityContext& Context)
{
	UAbilitySystemComponent* ASC = Context.SourceASC.Get();
	if (!ASC || !CueTag.IsValid())
	{
		return;
	}

	switch (Operation)
	{
	case EAKCueOperation::Execute:
		ASC->ExecuteGameplayCue(CueTag);
		break;
	case EAKCueOperation::Add:
		ASC->AddGameplayCue(CueTag);
		break;
	case EAKCueOperation::Remove:
		ASC->RemoveGameplayCue(CueTag);
		break;
	default:
		break;
	}
}

void UAKAbilityTask_SendGameplayCue::GetGASImpact(FAKGASImpact& OutImpact) const
{
	if (CueTag.IsValid())
	{
		OutImpact.Cues.AddUnique(CueTag);
	}
}

void UAKAbilityTask_SendGameplayCue::Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const
{
	Super::Validate(Asset, OutResult);

	if (!CueTag.IsValid())
	{
		OutResult.Add(EAKValidationSeverity::Error,
			LOCTEXT("NoCueTag", "SendGameplayCue 任务未指定 Cue Tag。"),
			const_cast<UAKAbilityTask_SendGameplayCue*>(this));
	}
}

void UAKAbilityTask_SendGameplayCue::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);

	OutJson->SetStringField(TEXT("cueTag"), CueTag.ToString());
	OutJson->SetStringField(TEXT("operation"),
		StaticEnum<EAKCueOperation>()->GetNameStringByValue(static_cast<int64>(Operation)));
}

void UAKAbilityTask_SendGameplayCue::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	FString TagName;
	if (InJson.TryGetStringField(TEXT("cueTag"), TagName))
	{
		CueTag = FGameplayTag::RequestGameplayTag(FName(*TagName), /*bErrorIfNotFound=*/false);
	}

	FString OpName;
	if (InJson.TryGetStringField(TEXT("operation"), OpName))
	{
		if (const UEnum* Enum = StaticEnum<EAKCueOperation>())
		{
			Operation = static_cast<EAKCueOperation>(Enum->GetValueByNameString(OpName));
		}
	}
}

#undef LOCTEXT_NAMESPACE
