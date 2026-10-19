#pragma once

#include "CoreMinimal.h"
#include "UAKGameplayAbility.h"
#include "UAKSampleAbility.generated.h"

class UGameplayEffect;

/**
 * 示例技能：UAKGameplayAbility 的最小子类。
 *
 * UAK 唯一要求的是 `AbilityAsset`（哪条时间轴）。
 * 冷却与消耗仍然走 GAS：
 * - `CooldownGameplayEffect`：内置字段，指向一个 HasDuration 且授予 Cooldown Tag 的 GE
 * - `CostGameplayEffect`：内置字段，指向一个 Instant 且把 Mana -N 的 GE
 *
 * 建议在蓝图里派生本类并填写这三个字段，然后把蓝图类填进角色的 DefaultAbilities。
 */
UCLASS()
class UAKSAMPLE_API UAKSampleAbility : public UAKGameplayAbility
{
	GENERATED_BODY()

public:
	UAKSampleAbility();

	/** 冷却 GE：Duration + GrantedTags(Cooldown.*) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample")
	TSubclassOf<UGameplayEffect> SampleCooldownEffect;

	/** 消耗 GE：Instant + Mana Additive 负值 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample")
	TSubclassOf<UGameplayEffect> SampleCostEffect;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
