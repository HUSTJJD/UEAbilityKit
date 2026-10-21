#include "UAKDemoAbility.h"

#include "UAKAbilityAsset.h"
#include "UAKDemoCharacter.h"

UAKDemoAbility::UAKDemoAbility()
{
	// UAK 执行器需要实例；预测需要 Local Predicted
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

UGameplayEffect* UAKDemoAbility::GetCooldownGameplayEffect() const
{
	return SampleCooldownEffect ? SampleCooldownEffect->GetDefaultObject<UGameplayEffect>() : nullptr;
}

UGameplayEffect* UAKDemoAbility::GetCostGameplayEffect() const
{
	return SampleCostEffect ? SampleCostEffect->GetDefaultObject<UGameplayEffect>() : nullptr;
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

	NotifyCharacterStarted(ActorInfo);

	// 之后交给 UAK：基类会创建执行器并按时间轴推进，跑完自动 EndAbility
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UAKDemoAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	NotifyCharacterEnded(ActorInfo, bWasCancelled);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAKDemoAbility::NotifyCharacterStarted(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!ActorInfo)
	{
		return;
	}

	if (AAKDemoCharacter* Character = Cast<AAKDemoCharacter>(ActorInfo->AvatarActor.Get()))
	{
		// 用资产名而不是类名：一次施放对应一条时间轴，UI 上更好认
		const FName Name = AbilityAsset ? FName(*AbilityAsset->GetName()) : GetClass()->GetFName();
		Character->K2_OnAbilityStarted(Name);
	}
}

void UAKDemoAbility::NotifyCharacterEnded(const FGameplayAbilityActorInfo* ActorInfo, bool bWasCancelled)
{
	if (!ActorInfo)
	{
		return;
	}

	if (AAKDemoCharacter* Character = Cast<AAKDemoCharacter>(ActorInfo->AvatarActor.Get()))
	{
		const FName Name = AbilityAsset ? FName(*AbilityAsset->GetName()) : GetClass()->GetFName();
		Character->K2_OnAbilityEnded(Name, bWasCancelled);
	}
}
