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
	SetCanTick(true);

	ChildSlot
	[
		SAssignNew(ListView, SListView<TSharedPtr<FAKBloodlineRow>>)
		.ListItemsSource(&Rows)
		.OnGenerateRow(this, &SUAKBloodlineView::GenerateRow)
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

	for (const UAKAbilityTrack* Track : AssetPtr->Tracks)
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

		for (const UAKAbilityTaskBase* Task : Track->Tasks)
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
			Rows.Add(MakeShared<FAKBloodlineRow>(TaskRow));

			for (const FGameplayTag& ImpactTag : Impact.Tags)
			{
				FAKBloodlineRow TagRow;
				TagRow.Label = FString::Printf(TEXT("Tag: %s"), *ImpactTag.ToString());
				TagRow.Depth = 3;
				Rows.Add(MakeShared<FAKBloodlineRow>(TagRow));
			}

			for (const TSoftClassPtr<UGameplayEffect>& Effect : Impact.Effects)
			{
				FAKBloodlineRow EffectRow;
				EffectRow.Label = FString::Printf(TEXT("GE: %s"), *Effect.ToString());
				EffectRow.Depth = 3;
				Rows.Add(MakeShared<FAKBloodlineRow>(EffectRow));
			}

			for (const FGameplayTag& Cue : Impact.Cues)
			{
				FAKBloodlineRow CueRow;
				CueRow.Label = FString::Printf(TEXT("Cue: %s"), *Cue.ToString());
				CueRow.Depth = 3;
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

	return SNew(STableRow<TSharedPtr<FAKBloodlineRow>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.6f)
			[
				SNew(STextBlock).Text(Label)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.4f)
			[
				SNew(STextBlock).Text(Detail).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
			]
		];
}

#undef LOCTEXT_NAMESPACE
