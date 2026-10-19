#pragma once

#include "CoreMinimal.h"
#include "UAKAbilityContext.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UAKAbilityTaskBase;

/**
 * 一次技能运行的上下文（对标 Able 的 UAblAbilityContext）。
 *
 * 设计要点：
 * - Context 是"运行数据"，Task 是无状态的，Task 的运行时状态一律放进 ScratchPads，
 *   这样同一个 AbilityAsset 可以被多个实体并行运行而互不干扰。
 * - 持有 SourceASC，让 Task 可以直接复用 GAS 原语（GE / Cue / Tag / AttributeSet）。
 */
USTRUCT(BlueprintType)
struct FAKAbilityContext
{
	GENERATED_BODY()

	/** 技能持有者（Avatar） */
	UPROPERTY(BlueprintReadWrite, Category = "Context")
	TWeakObjectPtr<AActor> Owner;

	/** 技能发起者（Instigator），用于伤害归因与溯源 */
	UPROPERTY(BlueprintReadWrite, Category = "Context")
	TWeakObjectPtr<AActor> Instigator;

	/** 目标集合（由目标选择 / AbilityTargetData 填充） */
	UPROPERTY(BlueprintReadWrite, Category = "Context")
	TArray<TWeakObjectPtr<AActor>> Targets;

	/** 施法者的 ASC，Task 通过它落地 GE / Cue */
	UPROPERTY(BlueprintReadWrite, Category = "Context")
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;

	/** 驱动本次运行的 GAS 技能（播放 Montage 等需要它） */
	UPROPERTY(BlueprintReadWrite, Category = "Context")
	TWeakObjectPtr<UGameplayAbility> Ability;

	/** 当前时间轴时间（秒，受 PlayRate 影响） */
	UPROPERTY(BlueprintReadWrite, Category = "Context")
	float CurrentTime = 0.f;

	/** 播放速率 */
	UPROPERTY(BlueprintReadWrite, Category = "Context")
	float PlayRate = 1.f;

	/** 本次运行是否在权威端（配合 UAKAbilityTaskBase::NetMode 决定任务是否执行） */
	UPROPERTY(BlueprintReadWrite, Category = "Context")
	bool bIsAuthority = false;

	/** 本次运行是否在本地控制端 */
	UPROPERTY(BlueprintReadWrite, Category = "Context")
	bool bIsLocallyControlled = false;

	/** Task 的运行时状态（Key = Task 实例，Value = ScratchPad） */
	UPROPERTY()
	TMap<UAKAbilityTaskBase*, UObject*> ScratchPads;

	void Reset()
	{
		Owner.Reset();
		Instigator.Reset();
		Targets.Reset();
		SourceASC.Reset();
		CurrentTime = 0.f;
		PlayRate = 1.f;
		ScratchPads.Reset();
	}
};
