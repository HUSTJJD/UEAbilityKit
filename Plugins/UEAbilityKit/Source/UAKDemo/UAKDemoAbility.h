#pragma once

#include "CoreMinimal.h"
#include "UAKGameplayAbility.h"
#include "UAKDemoAbility.generated.h"

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
class UAKDEMO_API UAKDemoAbility : public UAKGameplayAbility
{
	GENERATED_BODY()

public:
	UAKDemoAbility();

	/** 冷却 GE：Duration + GrantedTags(Cooldown.*) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample")
	TSubclassOf<UGameplayEffect> SampleCooldownEffect;

	/** 消耗 GE：Instant + Mana Additive 负值 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample")
	TSubclassOf<UGameplayEffect> SampleCostEffect;

	/**
	 * 用示例字段接管 GAS 内置的冷却 / 消耗。
	 * 这样蓝图里只需填 SampleCooldownEffect / SampleCostEffect 两个字段，
	 * 不必再去找 GAS 那两个同名内置字段（它们本就是给 C++ 覆写用的）。
	 */
	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;
	virtual UGameplayEffect* GetCostGameplayEffect() const override;

protected:
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
	/** 把开始 / 结束通知转发给角色（供 Lua 做 UI 联动） */
	void NotifyCharacterStarted(const FGameplayAbilityActorInfo* ActorInfo);
	void NotifyCharacterEnded(const FGameplayAbilityActorInfo* ActorInfo, bool bWasCancelled);
};
