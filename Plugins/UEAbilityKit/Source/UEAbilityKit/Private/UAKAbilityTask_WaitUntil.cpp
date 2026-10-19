#include "UAKAbilityTask_WaitUntil.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityContext.h"

#define LOCTEXT_NAMESPACE "UAKAbilityTask_WaitUntil"

UAKAbilityTask_WaitUntil::UAKAbilityTask_WaitUntil()
{
	// 等待本身就是"门控"的默认语义
	bGateTimeline = true;
	Duration = 1.f;
}

void UAKAbilityTask_WaitUntil::OnTaskStart(FAKAbilityContext& Context)
{
	// 条件可能已经满足：立刻放行（下一帧 EndDueTasks 会结束它）
}

bool UAKAbilityTask_WaitUntil::ShouldEndEarly(const FAKAbilityContext& Context) const
{
	return Condition.Evaluate(Context);
}

void UAKAbilityTask_WaitUntil::OnTaskEnd(FAKAbilityContext& Context, bool bInterrupted)
{
	// 门控解除：时间轴恢复推进（由执行器在下一帧重新判定 IsGated）
}

void UAKAbilityTask_WaitUntil::GetGASImpact(FAKGASImpact& OutImpact) const
{
	OutImpact.Summary = FText::Format(
		LOCTEXT("WaitSummary", "等待条件满足（最多 {0}s）{1}"),
		FText::AsNumber(Timeout),
		bGateTimeline ? LOCTEXT("GateOn", "· 冻结时间轴") : LOCTEXT("GateOff", ""));
}

void UAKAbilityTask_WaitUntil::Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const
{
	Super::Validate(Asset, OutResult);

	if (Condition.IsEmpty())
	{
		OutResult.Add(EAKValidationSeverity::Warning,
			LOCTEXT("EmptyWaitCondition", "WaitUntil 没有配置条件，等价于纯等待（Delay）。"),
			const_cast<UAKAbilityTask_WaitUntil*>(this));
	}

	if (Timeout <= 0.f && Condition.IsEmpty())
	{
		OutResult.Add(EAKValidationSeverity::Error,
			LOCTEXT("InfiniteWait", "WaitUntil 既没有条件也没有超时，会永久冻结时间轴。"),
			const_cast<UAKAbilityTask_WaitUntil*>(this));
	}
}

void UAKAbilityTask_WaitUntil::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);

	TSharedRef<FJsonObject> ConditionJson = MakeShared<FJsonObject>();
	Condition.ExportJson(ConditionJson);
	OutJson->SetObjectField(TEXT("condition"), ConditionJson);

	OutJson->SetNumberField(TEXT("timeout"), Timeout);
	OutJson->SetBoolField(TEXT("gateTimeline"), bGateTimeline);
	OutJson->SetBoolField(TEXT("treatTimeoutAsFailure"), bTreatTimeoutAsFailure);
}

void UAKAbilityTask_WaitUntil::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	const TSharedPtr<FJsonObject>* ConditionJson = nullptr;
	if (InJson.TryGetObjectField(TEXT("condition"), ConditionJson) && ConditionJson && ConditionJson->IsValid())
	{
		Condition.ImportJson(**ConditionJson);
	}

	double OutTimeout = Timeout;
	if (InJson.TryGetNumberField(TEXT("timeout"), OutTimeout))
	{
		Timeout = static_cast<float>(OutTimeout);
		Duration = Timeout;
	}

	InJson.TryGetBoolField(TEXT("gateTimeline"), bGateTimeline);
	InJson.TryGetBoolField(TEXT("treatTimeoutAsFailure"), bTreatTimeoutAsFailure);
}

#undef LOCTEXT_NAMESPACE
