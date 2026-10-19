#include "UAKGameplayAbility.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityContext.h"
#include "UAKAbilityInstance.h"
#include "UAKAbilityTickTask.h"
#include "UAKTargeting.h"
#include "UAKTrace.h"
#include "UEAbilityKitModule.h"

#include "AbilitySystemComponent.h"

UAKGameplayAbility::UAKGameplayAbility()
{
	// 沿用 GAS 标准策略，项目可按需求覆写。
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UAKGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!AbilityAsset)
	{
		UE_LOG(LogUAK, Warning, TEXT("UAKGameplayAbility '%s' 未指定 AbilityAsset。"), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	FAKAbilityContext Context;
	Context.Owner = ActorInfo->AvatarActor.Get();
	Context.Instigator = ActorInfo->OwnerActor.Get();
	Context.SourceASC = ActorInfo->AbilitySystemComponent.Get();
	Context.bIsAuthority = ActorInfo->IsNetAuthority();
	Context.bIsLocallyControlled = ActorInfo->IsLocallyControlled();
	Context.PlayRate = AbilityAsset->PlayRate;
	Context.Ability = this;

	// 目标来自 GAS 的 TargetData（由事件触发或目标选择任务产生），UAK 不自建目标选择系统
	if (TriggerEventData && TriggerEventData->TargetData.Num() > 0)
	{
		UAKTargeting::AppendTargetDataToContext(TriggerEventData->TargetData, Context);
	}

	// 溯源：由世界级记录器决定是否需要记录（关闭时为零开销）
	UAKTrace* Trace = nullptr;
	if (AActor* Avatar = ActorInfo->AvatarActor.Get())
	{
		if (UWorld* World = Avatar->GetWorld())
		{
			if (UAKTraceSubsystem* TraceSubsystem = World->GetSubsystem<UAKTraceSubsystem>())
			{
				Trace = TraceSubsystem->BeginTrace(AbilityAsset->GetName());
			}
		}
	}

	ActiveTrace = Trace;

	UAKAbilityInstance* Instance = NewObject<UAKAbilityInstance>(this);
	Instance->SetTrace(Trace);
	Instance->Initialize(AbilityAsset, Context);
	Instance->OnFinished.BindUObject(this, &UAKGameplayAbility::HandleTimelineFinished);
	ActiveInstance = Instance;

	if (Instance->IsFinished())
	{
		// 空技能（没有任务且长度为 0）：立即结束
		HandleTimelineFinished(/*bInterrupted=*/false);
		return;
	}

	// 由 AbilityTask 驱动时间轴：技能结束 → 任务自动结束 → 时间轴自动收尾
	UAKAbilityTickTask* TickTask = UAbilityTask::NewAbilityTask<UAKAbilityTickTask>(this, TEXT("UAKTimeline"));
	TickTask->SetInstance(Instance);
	TickTask->ReadyForActivation();
}

void UAKGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (ActiveInstance)
	{
		// 先解绑，避免 Finish() 回调再次触发 EndAbility 造成递归
		ActiveInstance->OnFinished.Unbind();
		ActiveInstance->Finish(bWasCancelled);
		ActiveInstance = nullptr;
	}

	EndTrace();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAKGameplayAbility::EndTrace()
{
	if (!ActiveTrace)
	{
		return;
	}

	if (AActor* Avatar = GetCurrentActorInfo() ? GetCurrentActorInfo()->AvatarActor.Get() : nullptr)
	{
		if (UWorld* World = Avatar->GetWorld())
		{
			if (UAKTraceSubsystem* TraceSubsystem = World->GetSubsystem<UAKTraceSubsystem>())
			{
				TraceSubsystem->EndTrace(ActiveTrace);
			}
		}
	}

	ActiveTrace = nullptr;
}

void UAKGameplayAbility::HandleTimelineFinished(bool bInterrupted)
{
	ActiveInstance = nullptr;

	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
			/*bReplicateEndAbility=*/true, bInterrupted);
	}
}
