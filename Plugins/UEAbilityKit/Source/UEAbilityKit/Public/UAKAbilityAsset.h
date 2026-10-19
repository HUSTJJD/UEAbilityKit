#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UAKTypes.h"
#include "UAKAbilityAsset.generated.h"

class UAKAbilityTrack;
class UAKAbilityTaskBase;
class UAnimMontage;

/**
 * 一条技能 = 一条时间轴 + 若干轨道 + 轨道上的任务。
 *
 * 三条设计底线：
 * 1. 纯数据资产（非 Blueprint）：可文本化导出、可 diff、可被引擎外工具与 CI 直接加载校验。
 * 2. 与 UAKGameplayAbility 解耦（N:M）：资产只描述"时间轴上发生什么"，
 *    冷却 / 消耗 / 目标获取 / 网络策略属于 Ability 的激活策略，不放在这里。
 * 3. 逻辑归 GAS：本资产不持有任何技能逻辑，任务只声明要施加什么 GE / Cue / Tag。
 */
UCLASS(BlueprintType)
class UEABILITYKIT_API UAKAbilityAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ── Meta ──────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
	FText Description;

	/** 资产版本，Json 导入时据此升级 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
	int32 Version = 1;

	// ── Timing ────────────────────────────────────────────────────────────
	/** 时间轴总长度（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Length = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float PlayRate = 1.f;

	/** 仅用于编辑器对齐与显示，不影响存储 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "1", UIMin = "1"))
	int32 FrameRate = 60;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (EditCondition = "bLoop", ClampMin = "0.0", UIMin = "0.0"))
	float LoopStartTime = 0.f;

	/** 0 表示无限循环 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (EditCondition = "bLoop", ClampMin = "0", UIMin = "0"))
	int32 MaxLoops = 0;

	// ── Tags（直接喂给 GAS）────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer AbilityTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer CancelAbilitiesWithTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer BlockAbilitiesWithTag;

	/** 激活该技能所需的 Tag（缺失则无法激活） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer RequiredTags;

	// ── 时间轴结构 ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Ability")
	TArray<UAKAbilityTrack*> Tracks;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	TArray<FAKTimeMarker> Markers;

#if WITH_EDITORONLY_DATA
	// ── 预览（不参与运行时，也不会被 Json 导出）────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preview")
	TSoftClassPtr<AActor> PreviewCharacterClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preview")
	TSoftObjectPtr<UAnimMontage> PreviewMontage;
#endif

	/** 收集所有轨道上的任务；bOnlyEnabledTracks 为 true 时跳过被关闭的轨道 */
	void CollectTasks(TArray<UAKAbilityTaskBase*>& OutTasks, bool bOnlyEnabledTracks = true) const;

	/**
	 * 生成执行计划：把"轨道的复合模式 + 重复次数"展开成一张扁平的调度表。
	 *
	 * Timeline：用任务自己的绝对时间；
	 * Sequence：按数组顺序首尾相接；
	 * Parallel：全部同时开始；
	 * Repeat：Sequence / Parallel 下把整条轨道重复 N 遍（Timeline 模式不支持重复）。
	 */
	void BuildExecutionPlan(TArray<FAKScheduledTask>& OutPlan, bool bOnlyEnabledTracks = true) const;

	/** 汇总全部任务的校验结果（含资产级校验） */
	void Validate(FAKValidationResult& OutResult) const;
};
