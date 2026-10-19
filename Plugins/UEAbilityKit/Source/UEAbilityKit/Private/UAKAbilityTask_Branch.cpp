#include "UAKAbilityTask_Branch.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityContext.h"
#include "UAKAbilityTrack.h"

#define LOCTEXT_NAMESPACE "UAKAbilityTask_Branch"

void UAKAbilityTask_Branch::OnTaskStart(FAKAbilityContext& Context)
{
	if (TargetTrackName.IsNone())
	{
		return;
	}

	const bool bPass = Condition.Evaluate(Context);

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
	for (const FGameplayTag& Tag : Condition.RequiredTags)
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

	if (Condition.IsEmpty())
	{
		OutResult.Add(EAKValidationSeverity::Warning,
			LOCTEXT("EmptyCondition", "Branch 任务没有配置任何条件，将永远判定为通过。"),
			const_cast<UAKAbilityTask_Branch*>(this));
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

	TSharedRef<FJsonObject> ConditionJson = MakeShared<FJsonObject>();
	Condition.ExportJson(ConditionJson);
	OutJson->SetObjectField(TEXT("condition"), ConditionJson);

	OutJson->SetStringField(TEXT("targetTrackName"), TargetTrackName.ToString());
	OutJson->SetBoolField(TEXT("enableTrack"), bEnableTrack);
	OutJson->SetBoolField(TEXT("applyOppositeOnFail"), bApplyOppositeOnFail);
}

void UAKAbilityTask_Branch::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	const TSharedPtr<FJsonObject>* ConditionJson = nullptr;
	if (InJson.TryGetObjectField(TEXT("condition"), ConditionJson) && ConditionJson && ConditionJson->IsValid())
	{
		Condition.ImportJson(**ConditionJson);
	}

	FString TrackName;
	if (InJson.TryGetStringField(TEXT("targetTrackName"), TrackName))
	{
		TargetTrackName = FName(*TrackName);
	}

	InJson.TryGetBoolField(TEXT("enableTrack"), bEnableTrack);
	InJson.TryGetBoolField(TEXT("applyOppositeOnFail"), bApplyOppositeOnFail);
}

#undef LOCTEXT_NAMESPACE
