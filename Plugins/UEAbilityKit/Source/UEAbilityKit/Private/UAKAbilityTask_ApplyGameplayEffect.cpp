#include "UAKAbilityTask_ApplyGameplayEffect.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityContext.h"
#include "UAKTrace.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

#define LOCTEXT_NAMESPACE "UAKAbilityTask_ApplyGameplayEffect"

UAKAbilityTask_ApplyGameplayEffect::UAKAbilityTask_ApplyGameplayEffect()
{
	// 这是一个单帧任务：进入即生效，不需要每帧驱动。
	Duration = 0.f;
}

void UAKAbilityTask_ApplyGameplayEffect::OnTaskStart(FAKAbilityContext& Context)
{
	UAbilitySystemComponent* SourceASC = Context.SourceASC.Get();
	if (!SourceASC || !EffectClass)
	{
		return;
	}

	// 收集目标：优先使用上下文里已有的目标集合（通常由 CollisionQuery 任务填充）
	TArray<AActor*> TargetActors;
	TargetActors.Reserve(Context.Targets.Num());
	for (const TWeakObjectPtr<AActor>& Target : Context.Targets)
	{
		if (Target.IsValid())
		{
			TargetActors.Add(Target.Get());
		}
	}

	if (TargetActors.Num() == 0)
	{
		if (bApplyToSelfIfNoTarget && Context.Owner.IsValid())
		{
			TargetActors.Add(Context.Owner.Get());
		}
		else
		{
			return;
		}
	}

	for (AActor* TargetActor : TargetActors)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (!TargetASC)
		{
			continue;
		}

		const FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
		const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(EffectClass, Level, EffectContext);
		if (!SpecHandle.IsValid())
		{
			continue;
		}

		// SetByCaller：让同一份资产在不同技能里复用不同数值
		for (const TPair<FName, float>& Pair : SetByCaller)
		{
			SpecHandle.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
		}

		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

		// 血缘下钻：GE → 目标，让"这 120 伤害哪来的"可一路追下去
		if (Context.Trace && Context.CurrentTaskNodeIndex != INDEX_NONE)
		{
			const int32 EffectNode = Context.Trace->AddNode(Context.CurrentTaskNodeIndex,
				EAKTraceNodeKind::Effect, EffectClass->GetName(), Context.CurrentTime);

			for (const TPair<FName, float>& Pair : SetByCaller)
			{
				Context.Trace->AddDetail(EffectNode,
					FString::Printf(TEXT("SetByCaller %s = %.2f"), *Pair.Key.ToString(), Pair.Value));
			}

			Context.Trace->AddNode(EffectNode, EAKTraceNodeKind::Target,
				TargetActor->GetName(), Context.CurrentTime);
		}
	}
}

void UAKAbilityTask_ApplyGameplayEffect::GetGASImpact(FAKGASImpact& OutImpact) const
{
	if (!EffectClass)
	{
		return;
	}

	OutImpact.Effects.AddUnique(TSoftClassPtr<UGameplayEffect>(EffectClass));

	FTextBuilder Builder;
	Builder.AppendLine(FText::FromString(EffectClass->GetName()));
	for (const TPair<FName, float>& Pair : SetByCaller)
	{
		Builder.AppendLine(FText::Format(LOCTEXT("SetByCallerLine", "  SetByCaller {0} = {1}"),
			FText::FromName(Pair.Key), FText::AsNumber(Pair.Value)));
	}
	OutImpact.Summary = Builder.ToText();
}

void UAKAbilityTask_ApplyGameplayEffect::Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const
{
	Super::Validate(Asset, OutResult);

	if (!EffectClass)
	{
		OutResult.Add(EAKValidationSeverity::Error,
			LOCTEXT("NoEffectClass", "ApplyGameplayEffect 任务未指定 GE。"),
			const_cast<UAKAbilityTask_ApplyGameplayEffect*>(this));
	}
}

void UAKAbilityTask_ApplyGameplayEffect::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);

	OutJson->SetStringField(TEXT("effectClass"), EffectClass ? EffectClass->GetPathName() : TEXT(""));
	OutJson->SetNumberField(TEXT("level"), Level);
	OutJson->SetBoolField(TEXT("applyToSelfIfNoTarget"), bApplyToSelfIfNoTarget);

	TSharedRef<FJsonObject> CallerObject = MakeShared<FJsonObject>();
	for (const TPair<FName, float>& Pair : SetByCaller)
	{
		CallerObject->SetNumberField(Pair.Key.ToString(), Pair.Value);
	}
	OutJson->SetObjectField(TEXT("setByCaller"), CallerObject);
}

void UAKAbilityTask_ApplyGameplayEffect::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	FString EffectPath;
	if (InJson.TryGetStringField(TEXT("effectClass"), EffectPath) && !EffectPath.IsEmpty())
	{
		EffectClass = LoadClass<UGameplayEffect>(nullptr, *EffectPath);
	}

	double OutLevel = Level;
	if (InJson.TryGetNumberField(TEXT("level"), OutLevel))
	{
		Level = static_cast<float>(OutLevel);
	}

	InJson.TryGetBoolField(TEXT("applyToSelfIfNoTarget"), bApplyToSelfIfNoTarget);

	const TSharedPtr<FJsonObject>* CallerObject = nullptr;
	if (InJson.TryGetObjectField(TEXT("setByCaller"), CallerObject) && CallerObject && CallerObject->IsValid())
	{
		SetByCaller.Reset();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*CallerObject)->Values)
		{
			SetByCaller.Add(FName(*Pair.Key), static_cast<float>(Pair.Value->AsNumber()));
		}
	}
}

#undef LOCTEXT_NAMESPACE
