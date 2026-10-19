#include "UAKDemoAbility.h"

UAKDemoAbility::UAKDemoAbility()
{
	// UAK 执行器需要实例；预测需要 Local Predicted
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UAKDemoAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 交给 GAS 走冷却与消耗：失败会自动拒绝激活
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// 之后交给 UAK：基类会创建执行器并按时间轴推进，跑完自动 EndAbility
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}
