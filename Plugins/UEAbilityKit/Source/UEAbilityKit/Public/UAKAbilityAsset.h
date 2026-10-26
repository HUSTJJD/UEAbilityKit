#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UAKChanneling.h"
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

	/** 被打断时，存活任务的结束顺序（见 EAKInterruptOrder） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	EAKInterruptOrder InterruptOrder = EAKInterruptOrder::ReverseStartOrder;

	// ── 引导 / 吟唱（Channeling）───────────────────────────────────────────
	/**
	 * 是否为引导技能：在窗口内持续检查条件，任一不满足即按 ChannelFailureAction 处理。
	 * 典型场景：蓄力技（松手打断）、站桩吟唱（移动打断）、锁定技（距离拉远打断）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channeling")
	bool bChanneled = false;

	/** 引导窗口起点（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channeling",
		meta = (EditCondition = "bChanneled", ClampMin = "0.0", UIMin = "0.0"))
	float ChannelingStartTime = 0.f;

	/** 引导窗口终点（秒）；-1 表示到技能结束 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channeling",
		meta = (EditCondition = "bChanneled", ClampMin = "-1.0"))
	float ChannelingEndTime = -1.f;

	/** 需要持续维持的条件（任一不满足即失败） */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Channeling",
		meta = (EditCondition = "bChanneled"))
	TArray<TObjectPtr<UAKChannelingCondition>> ChannelingConditions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channeling",
		meta = (EditCondition = "bChanneled"))
	EAKChannelFailureAction ChannelFailureAction = EAKChannelFailureAction::CancelAbility;

	/**
	 * 只在权威端判定引导失败。
	 * 取消技能是权威决策；客户端若需要即时反馈，用 Custom 条件 + Cue 自行处理。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Channeling",
		meta = (EditCondition = "bChanneled"))
	bool bEvaluateChannelingOnAuthorityOnly = true;

	// ── 时间轴结构 ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Ability")
	TArray<TObjectPtr<UAKAbilityTrack>> Tracks;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	TArray<FAKTimeMarker> Markers;

#if WITH_EDITORONLY_DATA
	// ── 预览（不参与运行时，也不会被 Json 导出）────────────────────────────
	/**
	 * 预览角色。留空 = 自动：项目默认 GameMode 的 DefaultPawnClass → 插件自带的示例角色。
	 * 填了才是"我要覆盖"，绝大多数资产不需要填。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preview",
		meta = (DisplayName = "预览角色（留空自动）"))
	TSoftClassPtr<AActor> PreviewCharacterClass;

	/** 预览 Montage。留空 = 自动：取资产里第一个 PlayMontage 任务的 Montage。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preview",
		meta = (DisplayName = "预览 Montage（留空自动）"))
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
