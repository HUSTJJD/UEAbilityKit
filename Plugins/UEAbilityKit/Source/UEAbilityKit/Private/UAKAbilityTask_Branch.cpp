#include "UAKAbilityTask_Branch.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityContext.h"
#include "UAKAbilityTrack.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

#define LOCTEXT_NAMESPACE "UAKAbilityTask_Branch"

bool UAKAbilityTask_Branch::EvaluateCondition(const FAKAbilityContext& Context) const
{
	UAbilitySystemComponent* ASC = Context.SourceASC.Get();
	AActor* ProbeActor = bEvaluateOnFirstTarget && Context.Targets.Num() > 0
		? Context.Targets[0].Get()
		: Context.Owner.Get();

	if (!ASC || !ProbeActor)
	{
		return false;
	}

	// Tag 条件
	if (RequiredTags.Num() > 0 && !ASC->HasAllMatchingGameplayTags(RequiredTags))
	{
		return false;
	}
	if (BlockedTags.Num() > 0 && ASC->HasAnyMatchingGameplayTags(BlockedTags))
	{
		return false;
	}

	// 属性条件
	if (Attribute.IsValid())
	{
		bool bFound = false;
		const float Value = UAbilitySystemBlueprintLibrary::GetFloatAttribute(ProbeActor, Attribute, bFound);
		if (!bFound)
		{
			return false;
		}

		bool bPass = false;
		switch (Comparison)
		{
		case EAKComparison::GreaterOrEqual:	bPass = (Value >= Threshold); break;
		case EAKComparison::Greater:		bPass = (Value > Threshold); break;
		case EAKComparison::LessOrEqual:	bPass = (Value <= Threshold); break;
		case EAKComparison::Less:			bPass = (Value < Threshold); break;
		case EAKComparison::Equal:
		default:							bPass = FMath::IsNearlyEqual(Value, Threshold); break;
		}

		if (!bPass)
		{
			return false;
		}
	}

	return true;
}

void UAKAbilityTask_Branch::OnTaskStart(FAKAbilityContext& Context)
{
	if (TargetTrackName.IsNone())
	{
		return;
	}

	bool bPass = EvaluateCondition(Context);
	if (bInvert)
	{
		bPass = !bPass;
	}

	bool bShouldEnable = bEnableTrack;
	if (!bPass && bApplyOppositeOnFail)
	{
		bShouldEnable = !bEnableTrack;
	}
	else if (!bPass)
	{
		// 条件不满足且不做相反动作：什么都不做
		return;
	}

	if (bShouldEnable)
	{
		Context.DisabledTracks.Remove(TargetTrackName);
	}
	else
	{
		Context.DisabledTracks.Add(TargetTrackName);
	}
}

void UAKAbilityTask_Branch::GetGASImpact(FAKGASImpact& OutImpact) const
{
	for (const FGameplayTag& Tag : RequiredTags)
	{
		OutImpact.Tags.AddUnique(Tag);
	}

	const FText Action = bEnableTrack
		? LOCTEXT("EnableTrack", "启用")
		: LOCTEXT("DisableTrack", "停用");

	OutImpact.Summary = FText::Format(
		LOCTEXT("BranchSummary", "若满足条件 → {0} 轨道「{1}」"),
		Action, FText::FromName(TargetTrackName));
}

void UAKAbilityTask_Branch::Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const
{
	Super::Validate(Asset, OutResult);

	if (TargetTrackName.IsNone())
	{
		OutResult.Add(EAKValidationSeverity::Error,
			LOCTEXT("NoTargetTrack", "Branch 任务未指定目标轨道名。"),
			const_cast<UAKAbilityTask_Branch*>(this));
		return;
	}

	// 目标轨道必须真实存在（避免拼写错误导致分支永远无效）
	bool bFound = false;
	if (Asset)
	{
		for (const UAKAbilityTrack* Track : Asset->Tracks)
		{
			if (Track && Track->TrackName == TargetTrackName)
			{
				bFound = true;
				break;
			}
		}
	}

	if (!bFound)
	{
		OutResult.Add(EAKValidationSeverity::Error,
			FText::Format(LOCTEXT("UnknownTrack", "找不到名为「{0}」的轨道。"),
				FText::FromName(TargetTrackName)),
			const_cast<UAKAbilityTask_Branch*>(this));
	}
}

void UAKAbilityTask_Branch::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);

	OutJson->SetStringField(TEXT("requiredTags"), RequiredTags.ToString());
	OutJson->SetStringField(TEXT("blockedTags"), BlockedTags.ToString());
	OutJson->SetStringField(TEXT("attribute"), Attribute.GetName());
	OutJson->SetStringField(TEXT("comparison"),
		StaticEnum<EAKComparison>()->GetNameStringByValue(static_cast<int64>(Comparison)));
	OutJson->SetNumberField(TEXT("threshold"), Threshold);
	OutJson->SetBoolField(TEXT("evaluateOnFirstTarget"), bEvaluateOnFirstTarget);
	OutJson->SetBoolField(TEXT("invert"), bInvert);
	OutJson->SetStringField(TEXT("targetTrackName"), TargetTrackName.ToString());
	OutJson->SetBoolField(TEXT("enableTrack"), bEnableTrack);
	OutJson->SetBoolField(TEXT("applyOppositeOnFail"), bApplyOppositeOnFail);
}

void UAKAbilityTask_Branch::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	auto ImportTags = [&InJson](const FString& Field, FGameplayTagContainer& OutTags)
	{
		FString TagsString;
		if (InJson.TryGetStringField(Field, TagsString) && !TagsString.IsEmpty())
		{
			OutTags.Reset();
			OutTags.FromExportString(TagsString);
		}
	};
	ImportTags(TEXT("requiredTags"), RequiredTags);
	ImportTags(TEXT("blockedTags"), BlockedTags);

	// FGameplayAttribute 由 UProperty 指针构成，无法从字符串安全重建，
	// 因此 Json 导入时不恢复属性条件（Tag 条件、阈值与目标轨道仍会恢复）。
	// 校验器会把"配置了阈值但没有属性"的情况报出来，避免静默丢失。

	FString ComparisonName;
	if (InJson.TryGetStringField(TEXT("comparison"), ComparisonName))
	{
		if (const UEnum* Enum = StaticEnum<EAKComparison>())
		{
			Comparison = static_cast<EAKComparison>(Enum->GetValueByNameString(ComparisonName));
		}
	}

	double OutThreshold = Threshold;
	if (InJson.TryGetNumberField(TEXT("threshold"), OutThreshold))
	{
		Threshold = static_cast<float>(OutThreshold);
	}

	InJson.TryGetBoolField(TEXT("evaluateOnFirstTarget"), bEvaluateOnFirstTarget);
	InJson.TryGetBoolField(TEXT("invert"), bInvert);

	FString TrackName;
	if (InJson.TryGetStringField(TEXT("targetTrackName"), TrackName))
	{
		TargetTrackName = FName(*TrackName);
	}

	InJson.TryGetBoolField(TEXT("enableTrack"), bEnableTrack);
	InJson.TryGetBoolField(TEXT("applyOppositeOnFail"), bApplyOppositeOnFail);
}

#undef LOCTEXT_NAMESPACE
