#include "SUAKDebuggerPanel.h"

#include "UAKDebugSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SUAKDebuggerPanel"

namespace
{
	/** 面板只关心 PIE 世界：编辑器世界不会跑技能 */
	UWorld* GetPIEWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World())
			{
				return Context.World();
			}
		}
		return nullptr;
	}
}

void SUAKDebuggerPanel::Construct(const FArguments& InArgs)
{
	bDrawEnabled = UAKDebugSubsystem::IsDebugDrawEnabled();

	ChildSlot
	[
		SNew(SVerticalBox)

		// ── 工具条：开关世界内标注 + 状态 ──────────────────────────────
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(this, &SUAKDebuggerPanel::GetToggleText)
				.OnClicked(this, &SUAKDebuggerPanel::OnToggleDrawClicked)
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.Padding(8.f, 0.f, 0.f, 0.f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(StatusText, STextBlock)
				.Text(this, &SUAKDebuggerPanel::GetStatusText)
			]
		]

		// ── 活跃技能列表 ──────────────────────────────────────────────
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(4.f, 0.f, 4.f, 4.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(ListView, SListView<TSharedPtr<FAKDebuggerRow>>)
				.ListItemsSource(&Rows)
				.OnGenerateRow(this, &SUAKDebuggerPanel::GenerateRow)
			]
		]
	];

	RebuildRows();

	// 每 0.1 秒刷新一次：足够跟得上时间轴，又不会明显占编辑器主线程
	RegisterActiveTimer(0.1f, FWidgetActiveTimerDelegate::CreateSP(this, &SUAKDebuggerPanel::Refresh));
}

TSharedRef<ITableRow> SUAKDebuggerPanel::GenerateRow(TSharedPtr<FAKDebuggerRow> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<SVerticalBox> Box = SNew(SVerticalBox);

	Box->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->OwnerName))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.85f, 1.f)))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(FText::FromString(Item->AbilityName))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock).Text(FText::FromString(Item->TimeText))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->StateText))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.75f, 0.3f)))
			]
		];

	for (const FString& Line : Item->TaskLines)
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(16.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Line))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f)))
		];
	}

	return SNew(STableRow<TSharedPtr<FAKDebuggerRow>>, OwnerTable)
		.Padding(2.f)
		[
			Box.ToSharedRef()
		];
}

EActiveTimerReturnType SUAKDebuggerPanel::Refresh(double InCurrentTime, float InDeltaTime)
{
	RebuildRows();
	return EActiveTimerReturnType::Continue;
}

void SUAKDebuggerPanel::RebuildRows()
{
	Rows.Reset();
	ActiveCount = 0;

	if (UWorld* PIEWorld = GetPIEWorld())
	{
		if (UAKDebugSubsystem* DebugSubsystem = PIEWorld->GetSubsystem<UAKDebugSubsystem>())
		{
			for (FAKDebugEntry& Entry : DebugSubsystem->BuildEntries())
			{
				TSharedPtr<FAKDebuggerRow> Row = MakeShared<FAKDebuggerRow>();
				Row->OwnerName = Entry.OwnerName;
				Row->AbilityName = Entry.AbilityName;
				Row->TimeText = FString::Printf(TEXT("%.2f / %.2fs"), Entry.CurrentTime, Entry.Length);

				if (Entry.bGated)
				{
					Row->StateText = TEXT("已冻结");
				}
				else if (Entry.LoopCount > 0)
				{
					Row->StateText = FString::Printf(TEXT("第 %d 轮"), Entry.LoopCount + 1);
				}

				Row->TaskLines = MoveTemp(Entry.ActiveTasks);
				Rows.Add(Row);
			}
			ActiveCount = Rows.Num();
		}
	}

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

FText SUAKDebuggerPanel::GetStatusText() const
{
	if (!GetPIEWorld())
	{
		return LOCTEXT("NotInPIE", "未进 PIE —— 点 Play 后这里会实时列出正在跑的技能");
	}

	if (ActiveCount == 0)
	{
		return LOCTEXT("Idle", "PIE 运行中，当前没有活跃技能");
	}

	return FText::Format(LOCTEXT("Active", "活跃技能 {0} 个（每 0.1 秒刷新）"), FText::AsNumber(ActiveCount));
}

FText SUAKDebuggerPanel::GetToggleText() const
{
	return bDrawEnabled
		? LOCTEXT("StopDraw", "关闭世界内标注")
		: LOCTEXT("StartDraw", "开启世界内标注");
}

FReply SUAKDebuggerPanel::OnToggleDrawClicked()
{
	bDrawEnabled = !bDrawEnabled;
	UAKDebugSubsystem::SetDebugDrawEnabled(bDrawEnabled);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
