#include "SUAKValidationBar.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SUAKValidationBar"

void SUAKValidationBar::Construct(const FArguments& InArgs)
{
	Asset = InArgs._Asset;
	OnIssueDoubleClicked = InArgs._OnIssueDoubleClicked;

	SetCanTick(true);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(HeaderText, STextBlock)
			.Text(this, &SUAKValidationBar::GetHeaderText)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SAssignNew(ListView, SListView<TSharedPtr<FAKValidationIssue>>)
			.ListItemsSource(&Issues)
			.OnGenerateRow(this, &SUAKValidationBar::GenerateRow)
			.OnMouseButtonDoubleClick(this, &SUAKValidationBar::OnRowDoubleClicked)
		]
	];

	Refresh();
}

FText SUAKValidationBar::GetHeaderText() const
{
	return FText::Format(LOCTEXT("Header", "⚠ {0} 警告 · {1} 错误"),
		FText::AsNumber(WarningCount), FText::AsNumber(ErrorCount));
}

void SUAKValidationBar::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	RefreshTimer += InDeltaTime;
	if (RefreshTimer < 0.5f)
	{
		return;
	}
	RefreshTimer = 0.f;

	// 用"任务数量 + 起止时间"做指纹，避免每帧全量校验
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		return;
	}

	FString Signature;
	for (const UAKAbilityTrack* Track : AssetPtr->Tracks)
	{
		if (!Track) { continue; }
		Signature += FString::Printf(TEXT("%d|"), Track->Tasks.Num());
		for (const UAKAbilityTaskBase* Task : Track->Tasks)
		{
			if (!Task) { continue; }
			Signature += FString::Printf(TEXT("%s:%.3f:%.3f;"), *Task->GetClass()->GetName(), Task->StartTime, Task->Duration);
		}
	}
	Signature += FString::Printf(TEXT("L%.3f"), AssetPtr->Length);

	if (Signature != LastSignature)
	{
		LastSignature = Signature;
		Refresh();
	}
}

void SUAKValidationBar::Refresh()
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		Issues.Reset();
		ErrorCount = 0;
		WarningCount = 0;
		if (ListView.IsValid()) { ListView->RequestListRefresh(); }
		return;
	}

	FAKValidationResult Result;
	AssetPtr->Validate(Result);

	Issues.Reset();
	Issues.Reserve(Result.Issues.Num());
	for (FAKValidationIssue& Issue : Result.Issues)
	{
		Issues.Add(MakeShared<FAKValidationIssue>(MoveTemp(Issue)));
	}

	ErrorCount = Result.NumErrors();
	WarningCount = Result.NumWarnings();

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SUAKValidationBar::GenerateRow(TSharedPtr<FAKValidationIssue> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (!Item.IsValid())
	{
		return SNew(STableRow<TSharedPtr<FAKValidationIssue>>, OwnerTable)
		[
			SNew(STextBlock).Text(FText::GetEmpty())
		];
	}

	const FLinearColor Color =
		(Item->Severity == EAKValidationSeverity::Error) ? FLinearColor(0.95f, 0.35f, 0.35f) :
		(Item->Severity == EAKValidationSeverity::Warning) ? FLinearColor(0.95f, 0.75f, 0.30f) :
		FLinearColor(0.65f, 0.65f, 0.65f);

	return SNew(STableRow<TSharedPtr<FAKValidationIssue>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(Item->Message)
			.ColorAndOpacity(Color)
		];
}

void SUAKValidationBar::OnRowDoubleClicked(TSharedPtr<FAKValidationIssue> Item)
{
	// 把出问题的任务交回上层定位（带参数才能跳到"哪一条"，资产级问题传 nullptr）
	if (Item.IsValid())
	{
		OnIssueDoubleClicked.ExecuteIfBound(Item->Task);
	}
}

#undef LOCTEXT_NAMESPACE
