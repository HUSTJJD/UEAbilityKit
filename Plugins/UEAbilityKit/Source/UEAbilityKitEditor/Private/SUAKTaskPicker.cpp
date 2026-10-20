#include "SUAKTaskPicker.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"

#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SUAKTaskPicker"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SUAKTaskPicker::Construct(const FArguments& InArgs)
{
	Asset = InArgs._Asset;
	OnAddTask = InArgs._OnAddTask;

	// 收集所有非抽象的 UAKAbilityTaskBase 子类（含蓝图派生）
	AllClasses.Reset();
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}
		if (!Class->IsChildOf(UAKAbilityTaskBase::StaticClass()) || Class == UAKAbilityTaskBase::StaticClass())
		{
			continue;
		}
		AllClasses.Add(Class);
	}
	AllClasses.Sort([](const UClass& A, const UClass& B)
	{
		return A.GetName() < B.GetName();
	});
	FilteredClasses = AllClasses;

	Refresh();

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.f)
		[
			SNew(STextBlock).Text(LOCTEXT("PickerTitle", "任务选择器"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.f)
		[
			SNew(SEditableTextBox)
			.HintText(LOCTEXT("SearchHint", "搜索任务…"))
			.OnTextChanged(this, &SUAKTaskPicker::OnSearchChanged)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(2.f)
		[
			SAssignNew(ClassListView, SListView<UClass*>)
			.ListItemsSource(&FilteredClasses)
			.OnGenerateRow(this, &SUAKTaskPicker::GenerateClassRow)
			.OnMouseButtonDoubleClick(this, &SUAKTaskPicker::OnClassDoubleClicked)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SComboBox<TSharedPtr<int32>>)
				.OptionsSource(&TrackOptions)
				.OnGenerateWidget(this, &SUAKTaskPicker::GenerateTrackComboItem)
				.OnSelectionChanged(this, &SUAKTaskPicker::OnTrackSelectionChanged)
				[
					SNew(STextBlock).Text(this, &SUAKTaskPicker::GetSelectedTrackText)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("AddTask", "添加"))
				.OnClicked(this, &SUAKTaskPicker::OnAddClicked)
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SUAKTaskPicker::Refresh()
{
	TrackOptions.Reset();
	SelectedTrack.Reset();

	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (AssetPtr)
	{
		for (int32 Index = 0; Index < AssetPtr->Tracks.Num(); ++Index)
		{
			TrackOptions.Add(MakeShared<int32>(Index));
		}
	}

	// 默认提供一个"新建轨道"选项
	TrackOptions.Add(MakeShared<int32>(INDEX_NONE));

	if (TrackOptions.Num() > 0)
	{
		SelectedTrack = TrackOptions[0];
	}
}

void SUAKTaskPicker::OnSearchChanged(const FText& Text)
{
	SearchText = Text.ToString();

	FilteredClasses.Reset();
	for (UClass* Class : AllClasses)
	{
		if (!Class)
		{
			continue;
		}
		if (SearchText.IsEmpty() || Class->GetName().Contains(SearchText))
		{
			FilteredClasses.Add(Class);
		}
	}

	if (ClassListView.IsValid())
	{
		ClassListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SUAKTaskPicker::GenerateClassRow(UClass* Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (!Item)
	{
		return SNew(STableRow<UClass*>, OwnerTable)
		[
			SNew(STextBlock).Text(LOCTEXT("InvalidClass", "-"))
		];
	}

	FText Label = FText::FromString(Item->GetName().Replace(TEXT("UAKAbilityTask_"), TEXT("")));
	FLinearColor Color = FLinearColor::White;

#if WITH_EDITOR
	// 用 CDO 取任务的编辑器元数据（分类 / 显示名 / 配色）
	if (const UAKAbilityTaskBase* DefaultTask = Item->GetDefaultObject<UAKAbilityTaskBase>())
	{
		Label = FText::Format(LOCTEXT("PickerRow", "{0} · {1}"),
			DefaultTask->GetTaskCategory(), DefaultTask->GetTaskDisplayName());
		Color = DefaultTask->GetTaskColor();
	}
#endif

	return SNew(STableRow<UClass*>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(Label)
			.ColorAndOpacity(Color)
		];
}

TSharedRef<SWidget> SUAKTaskPicker::GenerateTrackComboItem(TSharedPtr<int32> Item)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	FText Label;

	if (!Item.IsValid())
	{
		Label = LOCTEXT("InvalidTrack", "-");
	}
	else if (*Item == INDEX_NONE)
	{
		Label = LOCTEXT("NewTrack", "（新建轨道）");
	}
	else if (AssetPtr && AssetPtr->Tracks.IsValidIndex(*Item) && AssetPtr->Tracks[*Item])
	{
		Label = AssetPtr->Tracks[*Item]->GetDisplayName();
	}
	else
	{
		Label = FText::Format(LOCTEXT("TrackIndex", "轨道 {0}"), FText::AsNumber(*Item));
	}

	return SNew(STextBlock).Text(Label);
}

void SUAKTaskPicker::OnTrackSelectionChanged(TSharedPtr<int32> Item, ESelectInfo::Type SelectInfo)
{
	SelectedTrack = Item;
}

FText SUAKTaskPicker::GetSelectedTrackText() const
{
	UAKAbilityAsset* AssetPtr = Asset.Get();

	if (!SelectedTrack.IsValid() || !AssetPtr)
	{
		return LOCTEXT("NoTrack", "选择轨道");
	}

	if (*SelectedTrack == INDEX_NONE)
	{
		return LOCTEXT("NewTrackShort", "（新建轨道）");
	}

	if (AssetPtr->Tracks.IsValidIndex(*SelectedTrack) && AssetPtr->Tracks[*SelectedTrack])
	{
		return AssetPtr->Tracks[*SelectedTrack]->GetDisplayName();
	}

	return FText::Format(LOCTEXT("TrackIndexShort", "轨道 {0}"), FText::AsNumber(*SelectedTrack));
}

FReply SUAKTaskPicker::OnAddClicked()
{
	UClass* SelectedClass = nullptr;
	TArray<UClass*> SelectedItems = ClassListView.IsValid()
		? ClassListView->GetSelectedItems() : TArray<UClass*>();

	if (SelectedItems.Num() > 0)
	{
		SelectedClass = SelectedItems[0];
	}

	if (SelectedClass)
	{
		const int32 TrackIndex = SelectedTrack.IsValid() ? *SelectedTrack : INDEX_NONE;
		OnAddTask.ExecuteIfBound(SelectedClass, TrackIndex);
	}

	return FReply::Handled();
}

void SUAKTaskPicker::OnClassDoubleClicked(UClass* Item)
{
	if (Item)
	{
		const int32 TrackIndex = SelectedTrack.IsValid() ? *SelectedTrack : INDEX_NONE;
		OnAddTask.ExecuteIfBound(Item, TrackIndex);
	}
}

#undef LOCTEXT_NAMESPACE
