#pragma once

#include "CoreMinimal.h"
#include "UAKTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class UAKAbilityAsset;
class UAKAbilityTaskBase;

DECLARE_DELEGATE_TwoParams(FOnAKAddTask, TSubclassOf<UAKAbilityTaskBase> /*TaskClass*/, int32 /*TrackIndex*/);

/**
 * 任务选择器：树形/列表展示所有可用任务类（含蓝图派生），支持搜索与"加到哪条轨道"。
 *
 * 与 Able 的 Task Picker 一样属于通用能力（业界通用形态），
 * 但这里直接按 C++ 类的 DisplayName 展示，并允许一键指定目标轨道。
 */
class SUAKTaskPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUAKTaskPicker) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UAKAbilityAsset>, Asset)
		SLATE_EVENT(FOnAKAddTask, OnAddTask)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 资产变化后刷新轨道下拉 */
	void Refresh();

private:
	TSharedRef<ITableRow> GenerateClassRow(TSubclassOf<UAKAbilityTaskBase> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<SWidget> GenerateTrackComboItem(TSharedPtr<int32> Item);
	void OnSearchChanged(const FText& Text);
	void OnTrackSelectionChanged(TSharedPtr<int32> Item, ESelectInfo::Type SelectInfo);
	FReply OnAddClicked();
	void OnClassDoubleClicked(TSubclassOf<UAKAbilityTaskBase> Item);
	FText GetSelectedTrackText() const;

	TWeakObjectPtr<UAKAbilityAsset> Asset;
	FOnAKAddTask OnAddTask;

	TArray<TSubclassOf<UAKAbilityTaskBase>> AllClasses;
	TArray<TSubclassOf<UAKAbilityTaskBase>> FilteredClasses;
	TSharedPtr<SListView<TSubclassOf<UAKAbilityTaskBase>>> ClassListView;

	TArray<TSharedPtr<int32>> TrackOptions;
	TSharedPtr<int32> SelectedTrack;
	FString SearchText;
};
