#include "SUAKGASImpactBar.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTaskBase.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SUAKGASImpactBar"

void SUAKGASImpactBar::Construct(const FArguments& InArgs)
{
	Asset = InArgs._Asset;
	CursorTime = InArgs._CursorTime;
	OnEntryClicked = InArgs._OnEntryClicked;

	SetCanTick(true);

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
		[
			SAssignNew(SummaryText, STextBlock)
			.Text(this, &SUAKGASImpactBar::GetSummaryText)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SAssignNew(EntryBox, SHorizontalBox)
		]
	];
}

void SUAKGASImpactBar::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const float Time = CursorTime.Get(0.f);
	if (!FMath::IsNearlyEqual(Time, LastTime, 0.001f))
	{
		LastTime = Time;
		Recompute();
	}
}

void SUAKGASImpactBar::Recompute()
{
	EffectCount = 0;
	CueCount = 0;
	TagCount = 0;
	ActiveTasks.Reset();

	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		RebuildEntries();
		return;
	}

	// 与时间轴同一份执行计划：复合轨道展开后的真实时间
	AssetPtr->BuildExecutionPlan(CachedPlan, /*bOnlyEnabledTracks=*/true);

	const float Time = CursorTime.Get(0.f);
	for (const FAKScheduledTask& Scheduled : CachedPlan)
	{
		UAKAbilityTaskBase* Task = Scheduled.Task;
		if (!Task)
		{
			continue;
		}

		// 单帧任务的 EndTime 可能等于 StartTime，给一帧的容差
		const float Tolerance = 1.f / 60.f;
		const bool bActive = (Time >= Scheduled.StartTime - Tolerance)
			&& (Time <= FMath::Max(Scheduled.EndTime, Scheduled.StartTime) + Tolerance);
		if (!bActive)
		{
			continue;
		}

		FAKGASImpact Impact;
		Task->GetGASImpact(Impact);
		EffectCount += Impact.Effects.Num();
		CueCount += Impact.Cues.Num();
		TagCount += Impact.Tags.Num();

		// Repeat 轨道会展开出多条同名任务，条目只留一份
		ActiveTasks.AddUnique(TWeakObjectPtr<UAKAbilityTaskBase>(Task));
	}

	RebuildEntries();
}

void SUAKGASImpactBar::RebuildEntries()
{
	if (!EntryBox.IsValid())
	{
		return;
	}

	// 指纹不变就不动 UI：拖光标时大多数帧都命中同一批任务
	FString Signature;
	for (const TWeakObjectPtr<UAKAbilityTaskBase>& Task : ActiveTasks)
	{
		Signature += FString::Printf(TEXT("%s;"), *GetNameSafe(Task.Get()));
	}
	if (Signature == LastEntrySignature)
	{
		return;
	}
	LastEntrySignature = Signature;

	EntryBox->ClearChildren();

	for (const TWeakObjectPtr<UAKAbilityTaskBase>& TaskPtr : ActiveTasks)
	{
		UAKAbilityTaskBase* Task = TaskPtr.Get();
		if (!Task)
		{
			continue;
		}

		const float StartTime = Task->StartTime;
		const FText Label = FText::Format(
			LOCTEXT("EntryLabel", "{0} @ {1}s"),
			Task->GetTaskDisplayName(),
			FText::AsNumber(FMath::RoundToFloat(StartTime * 100.f) / 100.f));

		EntryBox->AddSlot()
		.AutoWidth()
		.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
		[
			SNew(SButton)
			.Text(Label)
			.ToolTipText(LOCTEXT("EntryTip", "点一下：跳到时间轴上的这个任务，并在血缘视图里展开它"))
			.OnClicked_Lambda([this, TaskPtr]()
			{
				OnEntryClicked.ExecuteIfBound(TaskPtr.Get());
				return FReply::Handled();
			})
		];
	}
}

FText SUAKGASImpactBar::GetSummaryText() const
{
	return FText::Format(LOCTEXT("ImpactSummary", "此刻生效 · GE {0} · Cue {1} · Tags {2} · 任务 {3}"),
		FText::AsNumber(EffectCount), FText::AsNumber(CueCount),
		FText::AsNumber(TagCount), FText::AsNumber(ActiveTasks.Num()));
}

#undef LOCTEXT_NAMESPACE
