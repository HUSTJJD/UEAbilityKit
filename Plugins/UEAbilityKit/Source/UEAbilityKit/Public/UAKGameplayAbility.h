#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "UAKGameplayAbility.generated.h"

class UAKAbilityAsset;
class UAKAbilityInstance;
class UAKTrace;

/**
 * UAK 与 GAS 的桥接点。
 *
 * 职责划分：
 * - GAS 负责：激活、目标数据、预测、复制、冷却（CooldownGE）、消耗（CostGE）、Tag 互斥。
 * - 本类负责：把 UAKAbilityAsset 这条时间轴跑起来（创建 UAKAbilityInstance，由 UAKAbilityTickTask 驱动）。
 *
 * 资产与技能解耦（N:M）：一个资产可被多个技能以不同冷却/消耗引用，
 * 一个技能也可按条件选择不同资产。
 */
UCLASS()
class UEABILITYKIT_API UAKGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UAKGameplayAbility();

	/** 要执行的技能时间轴资产 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Kit")
	UAKAbilityAsset* AbilityAsset = nullptr;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	/** 时间轴跑完或被中断时回调 */
	void HandleTimelineFinished(bool bInterrupted);

	/** 结束本次施放的溯源记录（可能为 null，表示未开启记录） */
	void EndTrace();

	UPROPERTY()
	UAKAbilityInstance* ActiveInstance = nullptr;

	UPROPERTY()
	UAKTrace* ActiveTrace = nullptr;
};
