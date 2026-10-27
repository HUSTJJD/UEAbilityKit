#pragma once

#include "CoreMinimal.h"
#include "UAKTypes.h"
#include "Widgets/SCompoundWidget.h"

class UAKAbilityAsset;
class UAKAbilityTaskBase;
class SHorizontalBox;
class STextBlock;

/** 点击影响条上的条目：把该任务交回上层（时间轴定位 + 血缘视图下钻） */
DECLARE_DELEGATE_OneParam(FOnAKImpactEntryClicked, UAKAbilityTaskBase* /*Task*/);

/**
 * GAS 影响条（UAK 独创）：随光标实时汇总"此刻生效的 Tag / 待施加 GE / Cue"。
 *
 * 数据来自各任务的 GetGASImpact()，因此不需要运行游戏就能看到这条技能会对 GAS 做什么。
 * Able 不使用 GAS，不可能有这个概念；这也是我们最核心的差异化之一。
 *
 * "此刻生效"必须与时间轴上画出来的片段是同一套时间：
 * 所以这里复用 BuildExecutionPlan（Sequence / Parallel / Repeat / StartDelay 展开后的真实时间），
 * 而不是直接读 Task->StartTime。
 */
class SUAKGASImpactBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUAKGASImpactBar) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UAKAbilityAsset>, Asset)
		SLATE_ATTRIBUTE(float, CursorTime)
		SLATE_EVENT(FOnAKImpactEntryClicked, OnEntryClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	FText GetSummaryText() const;
	void Recompute();
	/** 生效任务变了才重建条目，避免拖光标时每帧动 UI */
	void RebuildEntries();

	TWeakObjectPtr<UAKAbilityAsset> Asset;
	TAttribute<float> CursorTime;
	FOnAKImpactEntryClicked OnEntryClicked;

	TSharedPtr<STextBlock> SummaryText;
	TSharedPtr<SHorizontalBox> EntryBox;

	/** 与执行计划同源：''看到的''就是''跑出来的'' */
	TArray<FAKScheduledTask> CachedPlan;
	TArray<TWeakObjectPtr<UAKAbilityTaskBase>> ActiveTasks;

	int32 EffectCount = 0;
	int32 CueCount = 0;
	int32 TagCount = 0;
	float LastTime = -1.f;
	/** 上次条目的指纹：任务集合没变就不重建 */
	FString LastEntrySignature;
};
