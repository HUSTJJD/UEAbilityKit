#pragma once

#include "CoreMinimal.h"
#include "UAKAbilityTaskBase.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class UAKAbilityAsset;
class UAKTrace;

/** 双击某一行时：把该行的来源任务交回上层定位（时间轴 + Inspector） */
DECLARE_DELEGATE_OneParam(FOnAKBloodlineTaskFocused, UAKAbilityTaskBase* /*Task*/);

/** 血缘视图的一行（已扁平化，用 Depth 表示层级） */
struct FAKBloodlineRow
{
	FString Label;
	FString Detail;
	int32 Depth = 0;
	float Time = 0.f;
	bool bIsTask = false;

	/** 这一行的来源任务：双击时据此回跳时间轴（Tag / GE / Cue 子行也记，方便从结果找源头） */
	TWeakObjectPtr<UAKAbilityTaskBase> Task;
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
		SLATE_EVENT(FOnAKBloodlineTaskFocused, OnTaskFocusRequested)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** 切换到运行时实况数据源（由调试器调用） */
	void SetRuntimeTrace(const UAKTrace* InTrace);

private:
	TSharedRef<ITableRow> GenerateRow(TSharedPtr<FAKBloodlineRow> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnRowDoubleClicked(TSharedPtr<FAKBloodlineRow> Item);
	void RebuildFromAsset();
	void RebuildFromTrace(const UAKTrace* Trace);

	TWeakObjectPtr<UAKAbilityAsset> Asset;
	FOnAKBloodlineTaskFocused OnTaskFocusRequested;
	// UE 5.8 起 SListView 的 ItemType 必须是 UObject 指针 / TSharedPtr / TSharedRef / FName，
	// 裸结构体不再被 TListTypeTraits 接受，因此这里统一用共享指针。
	TArray<TSharedPtr<FAKBloodlineRow>> Rows;
	TSharedPtr<SListView<TSharedPtr<FAKBloodlineRow>>> ListView;
	TWeakObjectPtr<const UAKTrace> RuntimeTrace;

	float RefreshTimer = 0.f;
};
