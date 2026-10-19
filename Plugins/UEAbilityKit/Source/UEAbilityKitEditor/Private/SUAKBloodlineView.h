#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class UAKAbilityAsset;
class UAKTrace;

/** 血缘视图的一行（已扁平化，用 Depth 表示层级） */
struct FAKBloodlineRow
{
	FString Label;
	FString Detail;
	int32 Depth = 0;
	float Time = 0.f;
	bool bIsTask = false;
};

/**
 * 血缘视图（Explain）—— UAK 独创的第二个视图。
 *
 * 设计期：由各任务的 GetGASImpact() 静态汇总（不用跑游戏）。
 * 运行时：由 UAKTrace 写入真实数据（哪个 GE、改了哪个属性、落到哪个目标）。
 *
 * 价值：回答「这 120 点伤害是哪来的」——Able 只能告诉你"现在在跑哪个 Task"。
 */
class SUAKBloodlineView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUAKBloodlineView) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UAKAbilityAsset>, Asset)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** 切换到运行时实况数据源（由调试器调用） */
	void SetRuntimeTrace(const UAKTrace* InTrace);

private:
	TSharedRef<ITableRow> GenerateRow(FAKBloodlineRow Item, const TSharedRef<STableViewBase>& OwnerTable);
	void RebuildFromAsset();
	void RebuildFromTrace(const UAKTrace* Trace);

	TWeakObjectPtr<UAKAbilityAsset> Asset;
	TArray<FAKBloodlineRow> Rows;
	TSharedPtr<SListView<FAKBloodlineRow>> ListView;
	TWeakObjectPtr<const UAKTrace> RuntimeTrace;

	float RefreshTimer = 0.f;
};
