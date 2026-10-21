#include "UAKGameplayAbility.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityContext.h"
#include "UAKAbilityInstance.h"
#include "UAKAbilityTickTask.h"
#include "UAKDebugSubsystem.h"
#include "UAKTargeting.h"
#include "UAKTrace.h"
#include "UEAbilityKitModule.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"

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
		// 没有资产是**合法**的：本类是给别人当基类的。
		// 宿主工程（例如被改造进来的 Lyra）让自己的能力基类继承本类后，
		// 那些纯 GAS 技能（跳跃、被动）没有时间轴可跑，UAK 就该什么都不做，
		// 而不是把技能取消掉。
		UE_LOG(LogUAK, Verbose, TEXT("UAKGameplayAbility '%s' 未指定 AbilityAsset，按纯 GAS 技能运行。"), *GetName());
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
	if (UAKTraceSubsystem* TraceSubsystem = UWorld::GetSubsystem<UAKTraceSubsystem>(ActorInfo->AvatarActor->GetWorld()))
	{
		ActiveTrace = TraceSubsystem->BeginTrace(AbilityAsset->GetName());
	}

	ActiveInstance = NewObject<UAKAbilityInstance>(this);
	ActiveInstance->SetTrace(ActiveTrace);
	ActiveInstance->Initialize(AbilityAsset, Context);
	ActiveInstance->OnFinished.BindUObject(this, &UAKGameplayAbility::HandleTimelineFinished);

	// 登记到调试器（UAK.Debug.Summary / 世界内标注 / 编辑器面板的数据源）
	if (UAKDebugSubsystem* Debug = UWorld::GetSubsystem<UAKDebugSubsystem>(ActorInfo->AvatarActor->GetWorld()))
	{
		Debug->RegisterInstance(ActiveInstance);
		ActiveDebugSubsystem = Debug;
	}

	if (ActiveInstance->IsFinished())
	{
		// 空技能（没有任务且长度为 0）：立即结束
		HandleTimelineFinished(/*bInterrupted=*/false);
		return;
	}

	// 由 AbilityTask 驱动时间轴：技能结束 → 任务自动结束 → 时间轴自动收尾
	UAKAbilityTickTask* TickTask = UAbilityTask::NewAbilityTask<UAKAbilityTickTask>(this, TEXT("UAKTimeline"));
	TickTask->SetInstance(ActiveInstance);
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

		if (ActiveDebugSubsystem)
		{
			ActiveDebugSubsystem->UnregisterInstance(ActiveInstance);
			ActiveDebugSubsystem = nullptr;
		}

		ActiveInstance->Finish(bWasCancelled);
		ActiveInstance = nullptr;
	}

	EndTrace();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAKGameplayAbility::HandleTimelineFinished(bool bInterrupted)
{
	ActiveInstance = nullptr;
	EndTrace();

	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
			/*bReplicateEndAbility=*/true, bInterrupted);
	}
}

void UAKGameplayAbility::EndTrace()
{
	if (!ActiveTrace)
	{
		return;
	}

	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		if (UAKTraceSubsystem* TraceSubsystem = UWorld::GetSubsystem<UAKTraceSubsystem>(ActorInfo->AvatarActor->GetWorld()))
		{
			TraceSubsystem->EndTrace(ActiveTrace);
		}
	}
	ActiveTrace = nullptr;
}
