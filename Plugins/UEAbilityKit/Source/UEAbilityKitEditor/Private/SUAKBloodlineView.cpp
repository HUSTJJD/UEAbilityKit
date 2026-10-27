#include "SUAKBloodlineView.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"
#include "UAKTrace.h"
#include "UAKTypes.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SUAKBloodlineView"

void SUAKBloodlineView::Construct(const FArguments& InArgs)
{
	Asset = InArgs._Asset;
	OnTaskFocusRequested = InArgs._OnTaskFocusRequested;
	SetCanTick(true);

	ChildSlot
	[
		SAssignNew(ListView, SListView<TSharedPtr<FAKBloodlineRow>>)
		.ListItemsSource(&Rows)
		.OnGenerateRow(this, &SUAKBloodlineView::GenerateRow)
		.OnMouseButtonDoubleClick(this, &SUAKBloodlineView::OnRowDoubleClicked)
	];

	RebuildFromAsset();
}

void SUAKBloodlineView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	RefreshTimer += InDeltaTime;
	if (RefreshTimer < 0.5f)
	{
		return;
	}
	RefreshTimer = 0.f;

	if (const UAKTrace* Trace = RuntimeTrace.Get())
	{
		RebuildFromTrace(Trace);
	}
	else
	{
		RebuildFromAsset();
	}
}

void SUAKBloodlineView::SetRuntimeTrace(const UAKTrace* InTrace)
{
	RuntimeTrace = InTrace;
	if (InTrace)
	{
		RebuildFromTrace(InTrace);
	}
	else
	{
		RebuildFromAsset();
	}
}

void SUAKBloodlineView::RebuildFromAsset()
{
	Rows.Reset();

	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		if (ListView.IsValid()) { ListView->RequestListRefresh(); }
		return;
	}

	// 根：技能资产
	FAKBloodlineRow Root;
	Root.Label = AssetPtr->DisplayName.IsEmpty() ? AssetPtr->GetName() : AssetPtr->DisplayName.ToString();
	Root.Detail = FString::Printf(TEXT("长度 %.2fs"), AssetPtr->Length);
	Root.Depth = 0;
	Rows.Add(MakeShared<FAKBloodlineRow>(Root));

	for (UAKAbilityTrack* Track : AssetPtr->Tracks)
	{
		if (!Track)
		{
			continue;
		}

		FAKBloodlineRow TrackRow;
		TrackRow.Label = Track->GetDisplayName().ToString();
		TrackRow.Detail = Track->bEnabled ? TEXT("启用") : TEXT("已静音");
		TrackRow.Depth = 1;
		Rows.Add(MakeShared<FAKBloodlineRow>(TrackRow));

		for (UAKAbilityTaskBase* Task : Track->Tasks)
		{
			if (!Task)
			{
				continue;
			}

			FAKGASImpact Impact;
			Task->GetGASImpact(Impact);

			FAKBloodlineRow TaskRow;
			TaskRow.Label = Task->GetClass()->GetName().Replace(TEXT("UAKAbilityTask_"), TEXT(""));
			TaskRow.Detail = Impact.Summary.ToString();
			TaskRow.Time = Task->StartTime;
			TaskRow.Depth = 2;
			TaskRow.bIsTask = true;
			TaskRow.Task = Task;
			Rows.Add(MakeShared<FAKBloodlineRow>(TaskRow));

			for (const FGameplayTag& ImpactTag : Impact.Tags)
			{
				FAKBloodlineRow TagRow;
				TagRow.Label = FString::Printf(TEXT("Tag: %s"), *ImpactTag.ToString());
				TagRow.Depth = 3;
				TagRow.Task = Task;   // 从结果行也能回到"是谁给的这个 Tag"
				Rows.Add(MakeShared<FAKBloodlineRow>(TagRow));
			}

			for (const TSoftClassPtr<UGameplayEffect>& Effect : Impact.Effects)
			{
				FAKBloodlineRow EffectRow;
				EffectRow.Label = FString::Printf(TEXT("GE: %s"), *Effect.ToString());
				EffectRow.Depth = 3;
				EffectRow.Task = Task;
				Rows.Add(MakeShared<FAKBloodlineRow>(EffectRow));
			}

			for (const FGameplayTag& Cue : Impact.Cues)
			{
				FAKBloodlineRow CueRow;
				CueRow.Label = FString::Printf(TEXT("Cue: %s"), *Cue.ToString());
				CueRow.Depth = 3;
				CueRow.Task = Task;
				Rows.Add(MakeShared<FAKBloodlineRow>(CueRow));
			}
		}
	}

	if (ListView.IsValid()) { ListView->RequestListRefresh(); }
}

void SUAKBloodlineView::RebuildFromTrace(const UAKTrace* Trace)
{
	Rows.Reset();
	if (!Trace)
	{
		return;
	}

	const TArray<FAKTraceNode>& Nodes = Trace->GetNodes();
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const FAKTraceNode& Node = Nodes[Index];

		FAKBloodlineRow Row;
		Row.Label = Node.Label;
		Row.Time = Node.Time;
		Row.bIsTask = (Node.Kind == EAKTraceNodeKind::Task);

		// 由 ParentIndex 链计算缩进深度
		int32 Depth = 0;
		int32 Parent = Node.ParentIndex;
		while (Parent != INDEX_NONE && Depth < 8)
		{
			++Depth;
			Parent = Nodes.IsValidIndex(Parent) ? Nodes[Parent].ParentIndex : INDEX_NONE;
		}
		Row.Depth = Depth;

		if (Node.Details.Num() > 0)
		{
			Row.Detail = FString::Join(Node.Details, TEXT(" · "));
		}

		Rows.Add(MakeShared<FAKBloodlineRow>(Row));
	}

	if (ListView.IsValid()) { ListView->RequestListRefresh(); }
}

TSharedRef<ITableRow> SUAKBloodlineView::GenerateRow(TSharedPtr<FAKBloodlineRow> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (!Item.IsValid())
	{
		return SNew(STableRow<TSharedPtr<FAKBloodlineRow>>, OwnerTable)
		[
			SNew(STextBlock).Text(LOCTEXT("EmptyRow", ""))
		];
	}

	const FString Indent = FString::ChrN(Item->Depth * 3, TEXT(' '));
	const FText Label = FText::FromString(Indent + Item->Label);
	const FText Detail = Item->Time > 0.f
		? FText::Format(LOCTEXT("RowWithTime", "{0}  @ {1}s"), FText::FromString(Item->Detail), FText::AsNumber(Item->Time))
		: FText::FromString(Item->Detail);

	// 能回跳的行给个提示，否则双击没反应会让人以为是死的
	const bool bCanFocus = Item->Task.IsValid();

	return SNew(STableRow<TSharedPtr<FAKBloodlineRow>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.6f)
			[
				SNew(STextBlock)
				.Text(Label)
				.ToolTipText(bCanFocus
					? LOCTEXT("RowFocusTip", "双击：跳到时间轴上这个任务")
					: FText::GetEmpty())
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.4f)
			[
				SNew(STextBlock).Text(Detail).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
			]
		];
}

void SUAKBloodlineView::OnRowDoubleClicked(TSharedPtr<FAKBloodlineRow> Item)
{
	if (Item.IsValid() && Item->Task.IsValid())
	{
		OnTaskFocusRequested.ExecuteIfBound(Item->Task.Get());
	}
}

#undef LOCTEXT_NAMESPACE
