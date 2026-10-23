#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "UAKGameplayAbility.generated.h"

class UAKAbilityAsset;
class UAKAbilityInstance;
class UAKDebugSubsystem;
class UAKTrace;

/**
 * UAK 与 GAS 的桥接点，也是**宿主工程能力类的基类**。
 *
 * 职责划分：
 * - GAS 负责：激活、目标数据、预测、复制、冷却（CooldownGE）、消耗（CostGE）、Tag 互斥。
 * - 本类负责：把 UAKAbilityAsset 这条时间轴跑起来。
 *
 * 资产与技能解耦（N:M）：一个资产可被多个技能以不同冷却/消耗引用，
 * 一个技能也可按条件选择不同资产。
 *
 * ── 继承方向（重要）────────────────────────────────────────────
 * 是**宿主继承 UAK**，不是 UAK 去适配宿主：
 *
 *   class UMyGameGameplayAbility : public UAKGameplayAbility { ... }
 *
 * 这样插件保持完整、零依赖，能服务任何游戏；具体游戏（例如被改造进来的 Lyra）
 * 只是其中一个宿主。宿主侧只需要在自己的基类里加一句继承即可，
 * 已有的 ActivationPolicy / Cost / Camera 等机制原样保留。
 *
 * AbilityAsset 为空时本类什么都不做——纯 GAS 技能（跳跃、被动）照常工作，
 * 不会因为继承了这个基类就被取消。
 */
UCLASS()
class UEABILITYKIT_API UAKGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UAKGameplayAbility();
	// 宿主（Lyra）的能力类是带 FObjectInitializer 的构造：Super(ObjectInitializer)，
	// 基类没有这个重载就编不过
	UAKGameplayAbility(const FObjectInitializer& ObjectInitializer);

	/** 要执行的技能时间轴资产 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Kit")
	TObjectPtr<UAKAbilityAsset> AbilityAsset = nullptr;

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
	TObjectPtr<UAKAbilityInstance> ActiveInstance = nullptr;

	UPROPERTY()
	TObjectPtr<UAKTrace> ActiveTrace = nullptr;

	/** 本次施放登记到的调试器（结束技能时据此注销） */
	UPROPERTY()
	TObjectPtr<UAKDebugSubsystem> ActiveDebugSubsystem = nullptr;
};
