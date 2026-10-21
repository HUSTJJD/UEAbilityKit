#include "UAKAbilityTask_ApplyLooseTags.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityContext.h"
#include "UAKTrace.h"

#include "AbilitySystemComponent.h"

#define LOCTEXT_NAMESPACE "UAKAbilityTask_ApplyLooseTags"

UAKAbilityTask_ApplyLooseTags::UAKAbilityTask_ApplyLooseTags()
{
	// Tag 属于"判定状态"，默认只在权威端挂，靠 ASC 复制同步到各端；
	// 需要纯本地的表现标记时，把 NetMode 改成 Both 并把 bReplicate 关掉。
	NetMode = EAKTaskNetMode::ServerOnly;
}

void UAKAbilityTask_ApplyLooseTags::OnTaskStart(FAKAbilityContext& Context)
{
	UAbilitySystemComponent* ASC = Context.SourceASC.Get();
	if (!ASC || Tags.IsEmpty())
	{
		return;
	}

	const EGameplayTagReplicationState RepState = bReplicate
		? EGameplayTagReplicationState::TagAndCountToAll
		: EGameplayTagReplicationState::None;

	ASC->AddLooseGameplayTags(Tags, /*Count=*/1, RepState);

	if (Context.Trace && Context.CurrentTaskNodeIndex != INDEX_NONE)
	{
		const int32 Node = Context.Trace->AddNode(Context.CurrentTaskNodeIndex,
			EAKTraceNodeKind::Effect, TEXT("LooseTags"), Context.CurrentTime);

		TArray<FGameplayTag> TagArray;
		Tags.GetGameplayTagArray(TagArray);
		for (const FGameplayTag& Tag : TagArray)
		{
			Context.Trace->AddDetail(Node, FString::Printf(TEXT("+%s"), *Tag.ToString()));
		}
	}
}

void UAKAbilityTask_ApplyLooseTags::OnTaskEnd(FAKAbilityContext& Context, bool bInterrupted)
{
	UAbilitySystemComponent* ASC = Context.SourceASC.Get();
	if (!ASC || Tags.IsEmpty())
	{
		return;
	}

	if (bInterrupted && !bRemoveOnInterrupt)
	{
		return;
	}

	const EGameplayTagReplicationState RepState = bReplicate
		? EGameplayTagReplicationState::TagAndCountToAll
		: EGameplayTagReplicationState::None;

	ASC->RemoveLooseGameplayTags(Tags, /*Count=*/1, RepState);
}

void UAKAbilityTask_ApplyLooseTags::GetGASImpact(FAKGASImpact& OutImpact) const
{
	TArray<FGameplayTag> TagArray;
	Tags.GetGameplayTagArray(TagArray);

	for (const FGameplayTag& Tag : TagArray)
	{
		OutImpact.Tags.AddUnique(Tag);
	}

	if (TagArray.Num() > 0)
	{
		OutImpact.Summary = FText::Format(
			LOCTEXT("ImpactSummary", "限时 Tag：{0}（{1} ~ {2}s）"),
			FText::FromString(Tags.ToString()), FText::AsNumber(StartTime), FText::AsNumber(GetEndTime()));
	}
}

void UAKAbilityTask_ApplyLooseTags::Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const
{
	Super::Validate(Asset, OutResult);

	if (Tags.IsEmpty())
	{
		OutResult.Add(EAKValidationSeverity::Error,
			LOCTEXT("NoTags", "ApplyLooseTags 任务没有填任何 Tag。"),
			const_cast<UAKAbilityTask_ApplyLooseTags*>(this));
	}

	if (bReplicate && NetMode == EAKTaskNetMode::ClientOnly)
	{
		OutResult.Add(EAKValidationSeverity::Warning,
			LOCTEXT("ReplicateOnClientOnly", "客户端执行的任务挂上要复制的 Tag 会被服务端覆盖，检查 NetMode 配置。"),
			const_cast<UAKAbilityTask_ApplyLooseTags*>(this));
	}

	if (!bRemoveOnInterrupt)
	{
		OutResult.Add(EAKValidationSeverity::Info,
			LOCTEXT("KeepOnInterrupt", "被中断时保留 Tag：请确保有其它清理路径，否则 Tag 会残留。"),
			const_cast<UAKAbilityTask_ApplyLooseTags*>(this));
	}
}

void UAKAbilityTask_ApplyLooseTags::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);

	TArray<TSharedPtr<FJsonValue>> TagValues;
	TArray<FGameplayTag> TagArray;
	Tags.GetGameplayTagArray(TagArray);
	for (const FGameplayTag& Tag : TagArray)
	{
		TagValues.Add(MakeShared<FJsonValueString>(Tag.ToString()));
	}
	OutJson->SetArrayField(TEXT("tags"), TagValues);
	OutJson->SetBoolField(TEXT("replicate"), bReplicate);
	OutJson->SetBoolField(TEXT("removeOnInterrupt"), bRemoveOnInterrupt);
}

void UAKAbilityTask_ApplyLooseTags::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	Tags.Reset();
	TArray<FString> TagNames;
	if (InJson.TryGetStringArrayField(TEXT("tags"), TagNames))
	{
		for (const FString& TagName : TagNames)
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), /*bErrorIfNotFound=*/false);
			if (Tag.IsValid())
			{
				Tags.AddTag(Tag);
			}
		}
	}

	bool bOutReplicate = bReplicate;
	if (InJson.TryGetBoolField(TEXT("replicate"), bOutReplicate))
	{
		bReplicate = bOutReplicate;
	}

	bool bOutRemove = bRemoveOnInterrupt;
	if (InJson.TryGetBoolField(TEXT("removeOnInterrupt"), bOutRemove))
	{
		bRemoveOnInterrupt = bOutRemove;
	}
}

#undef LOCTEXT_NAMESPACE
