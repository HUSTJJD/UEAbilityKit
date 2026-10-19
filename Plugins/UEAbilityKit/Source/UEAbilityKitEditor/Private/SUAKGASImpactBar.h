#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UAKAbilityAsset;

/**
 * GAS 影响条（UAK 独创）：随光标实时汇总"此刻生效的 Tag / 待施加 GE / Cue / 目标数"。
 *
 * 数据来自各任务的 GetGASImpact()，因此不需要运行游戏就能看到这条技能会对 GAS 做什么。
 * Able 不使用 GAS，不可能有这个概念；这也是我们最核心的差异化之一。
 */
class SUAKGASImpactBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUAKGASImpactBar) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UAKAbilityAsset>, Asset)
		SLATE_ATTRIBUTE(float, CursorTime)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	FText GetSummaryText() const;
	void Recompute();

	TWeakObjectPtr<UAKAbilityAsset> Asset;
	TAttribute<float> CursorTime;

	TSharedPtr<class STextBlock> SummaryText;

	int32 EffectCount = 0;
	int32 CueCount = 0;
	int32 TagCount = 0;
	float LastTime = -1.f;
};
