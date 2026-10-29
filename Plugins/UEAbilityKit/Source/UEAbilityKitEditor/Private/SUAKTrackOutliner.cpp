#include "SUAKTrackOutliner.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTrack.h"
#include "UAKAbilityTaskBase.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Rendering/DrawElements.h"
#include "ScopedTransaction.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SUAKTrackOutliner"

namespace
{
	/** 普通 / 悬停 / 选中 三种行底（Sequencer 是分层 brush，我们用三档色，语义一样） */
	const FLinearColor ColorRowEven(0.043f, 0.043f, 0.043f);
	const FLinearColor ColorRowOdd(0.055f, 0.055f, 0.055f);
	const FLinearColor ColorRowHovered(0.09f, 0.09f, 0.09f);
	const FLinearColor ColorRowSelected(0.12f, 0.26f, 0.42f);
	const FLinearColor ColorDropIndicator(0.35f, 0.75f, 1.f);

	/** 类型标记字母：不引外部图标，避免 brush 缺失时显示空框 */
	TCHAR LetterForTrackType(EAKTrackType Type)
	{
		switch (Type)
		{
		case EAKTrackType::Main:			return TEXT('M');
		case EAKTrackType::Effect:			return TEXT('E');
		case EAKTrackType::Presentation:	return TEXT('P');
		case EAKTrackType::Query:			return TEXT('Q');
		case EAKTrackType::Motion:			return TEXT('R');   // Motion → "R"（位移）
		case EAKTrackType::Rule:			return TEXT('L');   // Rule → "L"（逻辑）
		case EAKTrackType::Custom:
		default:							return TEXT('C');
		}
	}

	FText BadgeForTrack(const UAKAbilityTrack& Track)
	{
		FString Badge;
		switch (Track.CompositeMode)
		{
		case EAKTrackCompositeMode::Sequence:	Badge = TEXT("Seq"); break;
		case EAKTrackCompositeMode::Parallel:	Badge = TEXT("Par"); break;
		case EAKTrackCompositeMode::Timeline:
		default:								Badge = TEXT(""); break;
		}

		// 重复次数只在非 Timeline 模式下生效（编排由轨道自己算时间）
		if (Track.CompositeMode != EAKTrackCompositeMode::Timeline && Track.RepeatCount > 1)
		{
			Badge = FString::Printf(TEXT("%s x%d"), *Badge, Track.RepeatCount);
		}

		if (Track.StartDelay > 0.f)
		{
			Badge += FString::Printf(TEXT(" +%.2fs"), Track.StartDelay);
		}

		Badge += FString::Printf(TEXT(" (%d)"), Track.Tasks.Num());
		return FText::FromString(Badge);
	}
}

FLinearColor UAKColorForTrackType(EAKTrackType Type)
{
	switch (Type)
	{
	case EAKTrackType::Main:			return FLinearColor(0.85f, 0.85f, 0.90f);
	case EAKTrackType::Effect:			return FLinearColor(0.95f, 0.35f, 0.35f);
	case EAKTrackType::Presentation:	return FLinearColor(0.40f, 0.80f, 0.95f);
	case EAKTrackType::Query:			return FLinearColor(0.55f, 0.85f, 0.45f);
	case EAKTrackType::Motion:			return FLinearColor(0.95f, 0.75f, 0.30f);
	case EAKTrackType::Rule:			return FLinearColor(0.75f, 0.50f, 0.95f);
	case EAKTrackType::Custom:
	default:							return FLinearColor(0.60f, 0.60f, 0.65f);
	}
}

// ───────────────────────────────────────────────────────────────────────────
// FAKTrackDragDropOp
// ───────────────────────────────────────────────────────────────────────────

TSharedRef<FAKTrackDragDropOp> FAKTrackDragDropOp::New(int32 InTrackIndex, FText InLabel)
{
	TSharedRef<FAKTrackDragDropOp> Op = MakeShared<FAKTrackDragDropOp>();
	Op->TrackIndex = InTrackIndex;
	Op->TrackLabel = MoveTemp(InLabel);
	Op->Construct();
	return Op;
}

TSharedPtr<SWidget> FAKTrackDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.9f))
		.Padding(FMargin(6.f, 2.f))
		[
			SNew(STextBlock).Text(TrackLabel)
		];
}

// ───────────────────────────────────────────────────────────────────────────
// FAKTrackMuteDragDropOp：横拖批量刷静音
// ───────────────────────────────────────────────────────────────────────────

TSharedRef<FAKTrackMuteDragDropOp> FAKTrackMuteDragDropOp::New(bool bInTargetEnabled)
{
	TSharedRef<FAKTrackMuteDragDropOp> Op = MakeShared<FAKTrackMuteDragDropOp>();
	Op->bTargetEnabled = bInTargetEnabled;
	// 整段拖拽一个事务：刷过多少行都只撤一次
	Op->Transaction = MakeUnique<FScopedTransaction>(LOCTEXT("BatchMute", "批量静音"));
	Op->Construct();
	return Op;
}

FAKTrackMuteDragDropOp::~FAKTrackMuteDragDropOp()
{
	// 析构即提交
	Transaction.Reset();
}

TSharedPtr<SWidget> FAKTrackMuteDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.12f, 0.12f, 0.12f, 0.9f))
		.Padding(FMargin(6.f, 2.f))
		[
			SNew(STextBlock).Text(bTargetEnabled
				? LOCTEXT("BatchUnmute", "开启")
				: LOCTEXT("BatchMuteHint", "静音"))
		];
}

// ───────────────────────────────────────────────────────────────────────────
// SUAKTrackRow
// ───────────────────────────────────────────────────────────────────────────

void SUAKTrackRow::Construct(const FArguments& InArgs)
{
	Asset = InArgs._Asset;
	TrackIndex = InArgs._TrackIndex;
	Outliner = InArgs._Outliner;

	const bool bLocked = Outliner.IsValid() && Outliner.Pin()->IsTrackLocked(TrackIndex);

	// 折叠 / 展开（其余三个按钮直接存成员，hover 淡入要用）
	TSharedPtr<SButton> CollapseButton;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(this, &SUAKTrackRow::GetRowBackgroundColor)
		.Padding(FMargin(0.f))
		[
			SNew(SHorizontalBox)

			// 折叠：折叠后时间轴只画一条汇总条
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.f, 0.f, 0.f, 0.f)
			[
				SAssignNew(CollapseButton, SButton)
				.ButtonColorAndOpacity(FLinearColor::Transparent)
				.ContentPadding(FMargin(2.f, 0.f))
				.Text(this, &SUAKTrackRow::GetCollapseGlyph)
				.ToolTipText(LOCTEXT("CollapseTip", "折叠 / 展开这条轨道"))
				.OnClicked_Lambda([this]()
				{
					if (TSharedPtr<SUAKTrackOutliner> Owner = Outliner.Pin())
					{
						Owner->ToggleCollapsed(TrackIndex);
					}
					return FReply::Handled();
				})
			]

			// 类型色条 = 静音开关（Sequencer 左侧 gutter 的思路：色条本身就承载状态）
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(this, &SUAKTrackRow::GetMuteColor)
				.ContentPadding(FMargin(0.f))
				.ToolTipText(LOCTEXT("MuteTip", "静音：关掉这条轨道（不参与执行）"))
				.OnClicked_Lambda([this]()
				{
					if (TSharedPtr<SUAKTrackOutliner> Owner = Outliner.Pin())
					{
						Owner->ToggleMute(TrackIndex);
					}
					return FReply::Handled();
				})
				[
					SNew(SBox).WidthOverride(8.f).HeightOverride(SUAKTrackOutliner::RowHeight - 10.f)
				]
			]

			// 类型标记
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(STextBlock)
				.Text(this, &SUAKTrackRow::GetTypeGlyph)
				.ColorAndOpacity(this, &SUAKTrackRow::GetTypeColor)
			]

			// 名字：双击就地重命名
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(NameText, SInlineEditableTextBlock)
				.Text(this, &SUAKTrackRow::GetTrackLabel)
				.IsReadOnly(bLocked)
				.ToolTipText(LOCTEXT("RenameTip", "双击改名"))
				.OnTextCommitted(this, &SUAKTrackRow::OnNameCommitted)
			]

			// 复合模式 / 任务数徽标
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f)
			[
				SNew(STextBlock)
				.Text(this, &SUAKTrackRow::GetBadgeText)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)))
			]

			// 说明：这里**没有 Solo 按钮**。技能不是多轨媒体，"只跑这一条"没有运行语义，
			// 做了也只是个空壳开关——要看单条轨道的效果，把别的轨道静音即可。

			// Lock
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(LockButton, SButton)
				.ButtonColorAndOpacity(FLinearColor::Transparent)
				.ForegroundColor(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda([this]()
					{
						return GetToggleColor(LockButton, IsLocked());
					})))
				.ContentPadding(FMargin(3.f, 0.f))
				.Text(LOCTEXT("Lock", "L"))
				.ToolTipText(LOCTEXT("LockTip", "锁定：禁止改动这条轨道"))
				.OnClicked_Lambda([this]()
				{
					if (TSharedPtr<SUAKTrackOutliner> Owner = Outliner.Pin())
					{
						Owner->ToggleLock(TrackIndex);
					}
					return FReply::Handled();
				})
			]

			// 删除
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 2.f, 0.f)
			[
				SAssignNew(DeleteButton, SButton)
				.ButtonColorAndOpacity(FLinearColor::Transparent)
				.ForegroundColor(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda([this]()
					{
						return GetToggleColor(DeleteButton, false);
					})))
				.ContentPadding(FMargin(3.f, 0.f))
				.Text(LOCTEXT("Remove", "X"))
				.ToolTipText(LOCTEXT("RemoveTip", "删除这条轨道"))
				.OnClicked_Lambda([this]()
				{
					if (TSharedPtr<SUAKTrackOutliner> Owner = Outliner.Pin())
					{
						Owner->RemoveTrack(TrackIndex);
					}
					return FReply::Handled();
				})
			]
		]
	];
}

int32 SUAKTrackRow::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	// 拖放的插入线：上/下半区各画一条，一眼看出会插到哪
	if (bDropTarget)
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();
		const float Y = bDropAbove ? 0.f : Size.Y - 2.f;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
			AllottedGeometry.ToPaintGeometry(
				UE::Slate::CastToVector2f(FVector2D(Size.X, 2.f)),
				FSlateLayoutTransform(UE::Slate::CastToVector2f(FVector2D(0.f, Y)))),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, ColorDropIndicator);
	}

	return LayerId + 2;
}

FReply SUAKTrackRow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (TSharedPtr<SUAKTrackOutliner> Owner = Outliner.Pin())
		{
			Owner->SelectTrack(TrackIndex);
		}
		// 顺手拿焦点，键盘才能接着操作这一行
		return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
	}
	return FReply::Unhandled();
}

FReply SUAKTrackRow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	TSharedPtr<SUAKTrackOutliner> Owner = Outliner.Pin();
	if (!Owner.IsValid())
	{
		return FReply::Unhandled();
	}

	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Up || Key == EKeys::Down)
	{
		const int32 Target = TrackIndex + (Key == EKeys::Up ? -1 : 1);
		if (Target < 0 || !Owner->IsValidTrackIndex(Target))
		{
			return FReply::Handled();
		}

		Owner->SelectTrack(Target);
		Owner->EnsureRowVisible(Target);

		// 把焦点交接到新行，才能连着按
		if (TSharedPtr<SUAKTrackRow> Next = Owner->GetRowWidget(Target))
		{
			FSlateApplication::Get().SetKeyboardFocus(Next);
		}
		return FReply::Handled();
	}

	if (Key == EKeys::F2)
	{
		EnterRenameMode();
		return FReply::Handled();
	}

	if (Key == EKeys::Delete || Key == EKeys::BackSpace)
	{
		Owner->RemoveTrack(TrackIndex);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SUAKTrackRow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (TSharedPtr<SUAKTrackOutliner> Owner = Outliner.Pin())
		{
			Owner->SelectTrack(TrackIndex);

			FWidgetPath WidgetPath = MouseEvent.GetEventPath() ? *MouseEvent.GetEventPath() : FWidgetPath();
			FSlateApplication::Get().PushMenu(
				SharedThis(this),
				WidgetPath,
				Owner->GenerateTrackContextMenu(TrackIndex),
				MouseEvent.GetScreenSpacePosition(),
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SUAKTrackRow::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bHovered = true;
}

void SUAKTrackRow::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	bHovered = false;
}

FReply SUAKTrackRow::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<SUAKTrackOutliner> Owner = Outliner.Pin();
	if (!Owner.IsValid() || Owner->IsTrackLocked(TrackIndex))
	{
		return FReply::Unhandled();
	}

	// 从行首色条拖出去 = 批量刷静音（Sequencer 的横拖 toggle）；其它位置 = 拖行重排
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	if (Local.X <= MuteGutterWidth)
	{
		const UAKAbilityAsset* AssetPtr = Asset.Get();
		const bool bNowEnabled = AssetPtr && AssetPtr->Tracks.IsValidIndex(TrackIndex)
			&& AssetPtr->Tracks[TrackIndex] && AssetPtr->Tracks[TrackIndex]->bEnabled;
		return FReply::Handled().BeginDragDrop(FAKTrackMuteDragDropOp::New(!bNowEnabled));
	}

	Owner->SelectTrack(TrackIndex);
	return FReply::Handled().BeginDragDrop(FAKTrackDragDropOp::New(TrackIndex, GetTrackLabel()));
}

FReply SUAKTrackRow::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	// 批量刷静音：扫过这一行就把它刷成目标状态（事务在拖拽操作里）
	if (TSharedPtr<FAKTrackMuteDragDropOp> MuteOp = DragDropEvent.GetOperationAs<FAKTrackMuteDragDropOp>())
	{
		if (TSharedPtr<SUAKTrackOutliner> Owner = Outliner.Pin())
		{
			Owner->SetMuteForDrag(TrackIndex, MuteOp->bTargetEnabled);
		}
		return FReply::Handled();
	}

	TSharedPtr<FAKTrackDragDropOp> Op = DragDropEvent.GetOperationAs<FAKTrackDragDropOp>();
	if (!Op.IsValid())
	{
		return FReply::Unhandled();
	}

	const FVector2D Local = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
	bDropTarget = true;
	bDropAbove = (Local.Y < MyGeometry.GetLocalSize().Y * 0.5f);

	return FReply::Handled();
}

void SUAKTrackRow::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	bDropTarget = false;
}

FReply SUAKTrackRow::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	// 批量刷静音：事务随拖拽操作析构提交；这里把"结果"同步给时间轴（执行计划要重算）
	if (DragDropEvent.GetOperationAs<FAKTrackMuteDragDropOp>().IsValid())
	{
		if (TSharedPtr<SUAKTrackOutliner> Owner = Outliner.Pin())
		{
			Owner->NotifyTracksChangedExternal();
		}
		return FReply::Handled();
	}

	TSharedPtr<FAKTrackDragDropOp> Op = DragDropEvent.GetOperationAs<FAKTrackDragDropOp>();
	if (!Op.IsValid() || !Outliner.IsValid())
	{
		bDropTarget = false;
		return FReply::Unhandled();
	}

	const FVector2D Local = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
	const bool bAbove = (Local.Y < MyGeometry.GetLocalSize().Y * 0.5f);
	bDropTarget = false;

	TSharedPtr<SUAKTrackOutliner> Owner = Outliner.Pin();

	const int32 From = Op->TrackIndex;
	int32 Target = TrackIndex + (bAbove ? 0 : 1);
	if (From < Target)
	{
		// 源轨道先被摘掉，它后面的下标会整体前移一位
		--Target;
	}

	if (Owner.IsValid() && From != Target)
	{
		Owner->ReorderTrack(From, Target);
	}

	return FReply::Handled();
}

void SUAKTrackRow::EnterRenameMode()
{
	if (NameText.IsValid())
	{
		NameText->EnterEditingMode();
	}
}

FSlateColor SUAKTrackRow::GetRowBackgroundColor() const
{
	TSharedPtr<SUAKTrackOutliner> Owner = Outliner.Pin();
	if (Owner.IsValid() && Owner->GetSelectedTrackIndex() == TrackIndex)
	{
		return FSlateColor(ColorRowSelected);
	}
	if (bHovered)
	{
		return FSlateColor(ColorRowHovered);
	}
	// 锁定的行压暗一点，和"能改"的行区分开
	return FSlateColor((TrackIndex % 2 == 0) ? ColorRowEven : ColorRowOdd);
}

FSlateColor SUAKTrackRow::GetToggleColor(const TSharedPtr<SWidget>& Widget, bool bActive) const
{
	// 抄 Sequencer 的五档透明度：激活最亮，hover 按钮次之，只 hover 行时"预览"，无关近乎隐形
	float Alpha = 0.1f;
	if (bActive)
	{
		Alpha = 1.f;
	}
	else if (Widget.IsValid() && Widget->IsHovered())
	{
		Alpha = 0.8f;
	}
	else if (bHovered)
	{
		Alpha = 0.25f;
	}
	return FSlateColor(FLinearColor(1.f, 1.f, 1.f, Alpha));
}

FText SUAKTrackRow::GetTrackLabel() const
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(TrackIndex) || !AssetPtr->Tracks[TrackIndex])
	{
		return FText::GetEmpty();
	}
	return AssetPtr->Tracks[TrackIndex]->GetDisplayName();
}

FText SUAKTrackRow::GetBadgeText() const
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(TrackIndex) || !AssetPtr->Tracks[TrackIndex])
	{
		return FText::GetEmpty();
	}
	return BadgeForTrack(*AssetPtr->Tracks[TrackIndex]);
}

FText SUAKTrackRow::GetTypeGlyph() const
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(TrackIndex) || !AssetPtr->Tracks[TrackIndex])
	{
		return FText::GetEmpty();
	}
	return FText::FromString(FString::Chr(LetterForTrackType(AssetPtr->Tracks[TrackIndex]->TrackType)));
}

FSlateColor SUAKTrackRow::GetTypeColor() const
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(TrackIndex) || !AssetPtr->Tracks[TrackIndex])
	{
		return FSlateColor(FLinearColor::Gray);
	}
	return FSlateColor(UAKColorForTrackType(AssetPtr->Tracks[TrackIndex]->TrackType));
}

FSlateColor SUAKTrackRow::GetMuteColor() const
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(TrackIndex) || !AssetPtr->Tracks[TrackIndex])
	{
		return FSlateColor(FLinearColor::Gray);
	}

	const UAKAbilityTrack* Track = AssetPtr->Tracks[TrackIndex];
	return FSlateColor(Track->bEnabled ? UAKColorForTrackType(Track->TrackType) : FLinearColor(0.25f, 0.25f, 0.25f));
}

FText SUAKTrackRow::GetCollapseGlyph() const
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(TrackIndex) || !AssetPtr->Tracks[TrackIndex])
	{
		return FText::GetEmpty();
	}
	return FText::FromString(AssetPtr->Tracks[TrackIndex]->bCollapsed ? TEXT("+") : TEXT("-"));
}

bool SUAKTrackRow::IsLocked() const
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	return AssetPtr && AssetPtr->Tracks.IsValidIndex(TrackIndex) && AssetPtr->Tracks[TrackIndex]
		&& AssetPtr->Tracks[TrackIndex]->bLocked;
}

void SUAKTrackRow::OnNameCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnCleared)
	{
		return;
	}

	if (TSharedPtr<SUAKTrackOutliner> Owner = Outliner.Pin())
	{
		Owner->RenameTrack(TrackIndex, NewText.ToString());
	}
}

// ───────────────────────────────────────────────────────────────────────────
// SUAKTrackOutliner
// ───────────────────────────────────────────────────────────────────────────

void SUAKTrackOutliner::Construct(const FArguments& InArgs)
{
	Asset = InArgs._Asset;
	OnTrackSelected = InArgs._OnTrackSelected;
	OnTracksChanged = InArgs._OnTracksChanged;
	OnScrollOffsetChanged = InArgs._OnScrollOffsetChanged;

	ChildSlot
	[
		SAssignNew(ScrollBox, SScrollBox)
		.Orientation(Orient_Vertical)
		.OnUserScrolled(this, &SUAKTrackOutliner::HandleUserScrolled)
		// SScrollBox 是多槽容器：内容要放在 Slot 里，不是 ChildSlot
		+ SScrollBox::Slot()
		[
			SAssignNew(RowsBox, SVerticalBox)
		]
	];

	// 行需要 SharedThis(this)（弱引用回 Outliner），Construct 里还拿不到，首帧再建
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SUAKTrackOutliner::InitialRefresh));
}

EActiveTimerReturnType SUAKTrackOutliner::InitialRefresh(double InCurrentTime, float InDeltaTime)
{
	Refresh();
	return EActiveTimerReturnType::Stop;
}

FReply SUAKTrackOutliner::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::RightMouseButton)
	{
		return FReply::Unhandled();
	}

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Ctx_AddTrack", "新建轨道"),
		LOCTEXT("Ctx_AddTrackTip", "在末尾添加一条轨道"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { AddTrack(); })));

	FWidgetPath WidgetPath = MouseEvent.GetEventPath() ? *MouseEvent.GetEventPath() : FWidgetPath();
	FSlateApplication::Get().PushMenu(
		SharedThis(this),
		WidgetPath,
		MenuBuilder.MakeWidget(),
		MouseEvent.GetScreenSpacePosition(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

	return FReply::Handled();
}

void SUAKTrackOutliner::HandleUserScrolled(float InScrollOffset)
{
	ScrollOffset = InScrollOffset;
	OnScrollOffsetChanged.ExecuteIfBound(ScrollOffset);
}

void SUAKTrackOutliner::Refresh()
{
	if (!RowsBox.IsValid())
	{
		return;
	}

	RowsBox->ClearChildren();
	Rows.Reset();

	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		return;
	}

	const int32 RenameIndex = PendingRenameIndex;
	PendingRenameIndex = INDEX_NONE;
	TSharedPtr<SUAKTrackRow> RenameRow;

	// 一条轨道都没有时给句人话，别让人以为编辑器坏了
	if (AssetPtr->Tracks.Num() == 0)
	{
		RowsBox->AddSlot()
		.AutoHeight()
		.Padding(8.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("EmptyOutliner", "还没有轨道。\n点工具条「添加轨道」，或在这里右键新建。"))
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
		];
		return;
	}

	for (int32 Index = 0; Index < AssetPtr->Tracks.Num(); ++Index)
	{
		TSharedPtr<SUAKTrackRow> Row = SNew(SUAKTrackRow)
			.Asset(Asset)
			.TrackIndex(Index)
			.Outliner(SharedThis(this));

		RowsBox->AddSlot()
		.AutoHeight()
		[
			SNew(SBox).HeightOverride(RowHeight)
			[
				Row.ToSharedRef()
			]
		];

		Rows.Add(Row);

		if (Index == RenameIndex)
		{
			RenameRow = Row;
		}
	}

	// 新建完立刻能改名字（Persona 的 PendingRenameTrackIndex）
	if (RenameRow.IsValid())
	{
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda(
			[WeakRow = TWeakPtr<SUAKTrackRow>(RenameRow)](double, float)
			{
				if (TSharedPtr<SUAKTrackRow> Row = WeakRow.Pin())
				{
					Row->EnterRenameMode();
				}
				return EActiveTimerReturnType::Stop;
			}));
	}
}

void SUAKTrackOutliner::ScrollBy(float DeltaPixels)
{
	if (!ScrollBox.IsValid())
	{
		return;
	}

	ScrollBox->SetScrollOffset(FMath::Max(0.f, ScrollOffset + DeltaPixels));
	ScrollOffset = ScrollBox->GetScrollOffset();
	OnScrollOffsetChanged.ExecuteIfBound(ScrollOffset);
}

void SUAKTrackOutliner::ScrollToRow(int32 Row)
{
	if (!ScrollBox.IsValid() || Row < 0)
	{
		return;
	}

	// 把目标行放到上方第三行的位置：上下都留得下上下文
	ScrollBox->SetScrollOffset(FMath::Max(0.f, Row * RowHeight - RowHeight * 2.f));
	ScrollOffset = ScrollBox->GetScrollOffset();
	OnScrollOffsetChanged.ExecuteIfBound(ScrollOffset);
}

void SUAKTrackOutliner::SetSelectedTrackIndex(int32 Index)
{
	SelectedTrackIndex = Index;
}

void SUAKTrackOutliner::SelectTrack(int32 Index)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(Index))
	{
		return;
	}

	SelectedTrackIndex = Index;
	OnTrackSelected.ExecuteIfBound(AssetPtr->Tracks[Index], Index);
}

bool SUAKTrackOutliner::IsTrackLocked(int32 Index) const
{
	const UAKAbilityAsset* AssetPtr = Asset.Get();
	return AssetPtr && AssetPtr->Tracks.IsValidIndex(Index) && AssetPtr->Tracks[Index]
		&& AssetPtr->Tracks[Index]->bLocked;
}

bool SUAKTrackOutliner::IsValidTrackIndex(int32 Index) const
{
	const UAKAbilityAsset* AssetPtr = Asset.Get();
	return AssetPtr && AssetPtr->Tracks.IsValidIndex(Index);
}

TSharedPtr<SUAKTrackRow> SUAKTrackOutliner::GetRowWidget(int32 Index) const
{
	return Rows.IsValidIndex(Index) ? Rows[Index].Pin() : TSharedPtr<SUAKTrackRow>();
}

void SUAKTrackOutliner::EnsureRowVisible(int32 Row)
{
	if (!ScrollBox.IsValid() || Row < 0)
	{
		return;
	}

	const float ViewHeight = ScrollBox->GetCachedGeometry().GetLocalSize().Y;
	if (ViewHeight <= 0.f)
	{
		return;
	}

	const float RowTop = Row * RowHeight;
	const float RowBottom = RowTop + RowHeight;

	// 已经看得见就别动：键盘连按时画面跳来跳去很烦
	if (RowTop >= ScrollOffset && RowBottom <= ScrollOffset + ViewHeight)
	{
		return;
	}

	if (RowTop < ScrollOffset)
	{
		ScrollBox->SetScrollOffset(RowTop);
	}
	else
	{
		ScrollBox->SetScrollOffset(RowBottom - ViewHeight);
	}

	ScrollOffset = ScrollBox->GetScrollOffset();
	OnScrollOffsetChanged.ExecuteIfBound(ScrollOffset);
}

void SUAKTrackOutliner::NotifyChanged()
{
	Refresh();
	OnTracksChanged.ExecuteIfBound();
}

void SUAKTrackOutliner::ReorderTrack(int32 FromIndex, int32 ToIndex)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(FromIndex) || !AssetPtr->Tracks.IsValidIndex(ToIndex))
	{
		return;
	}
	if (IsTrackLocked(FromIndex) || IsTrackLocked(ToIndex))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ReorderTrack", "重排轨道"));
	AssetPtr->Modify();

	UAKAbilityTrack* Moved = AssetPtr->Tracks[FromIndex];
	AssetPtr->Tracks.RemoveAt(FromIndex);
	AssetPtr->Tracks.Insert(Moved, ToIndex);
	AssetPtr->MarkPackageDirty();

	if (SelectedTrackIndex == FromIndex)
	{
		SelectedTrackIndex = ToIndex;
	}

	NotifyChanged();
}

void SUAKTrackOutliner::MoveTrack(int32 Index, int32 Delta)
{
	const int32 Target = Index + Delta;
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(Target))
	{
		return;
	}
	ReorderTrack(Index, Target);
}

void SUAKTrackOutliner::AddTrack(int32 InsertIndex)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddTrack", "添加轨道"));
	AssetPtr->Modify();

	UAKAbilityTrack* Track = NewObject<UAKAbilityTrack>(AssetPtr);
	Track->TrackName = FName(*FString::Printf(TEXT("Track_%d"), AssetPtr->Tracks.Num()));
	Track->TrackType = EAKTrackType::Custom;
	Track->CompositeMode = EAKTrackCompositeMode::Timeline;

	const int32 At = AssetPtr->Tracks.IsValidIndex(InsertIndex) ? InsertIndex : AssetPtr->Tracks.Num();
	AssetPtr->Tracks.Insert(Track, At);
	AssetPtr->MarkPackageDirty();

	SelectedTrackIndex = At;
	PendingRenameIndex = At;   // 新建即重命名
	NotifyChanged();
	OnTrackSelected.ExecuteIfBound(Track, At);
}

void SUAKTrackOutliner::DuplicateTrack(int32 Index)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(Index) || IsTrackLocked(Index))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DuplicateTrack", "复制轨道"));
	AssetPtr->Modify();

	UAKAbilityTrack* Source = AssetPtr->Tracks[Index];
	UAKAbilityTrack* Copy = DuplicateObject<UAKAbilityTrack>(Source, AssetPtr);
	if (!Copy)
	{
		return;
	}

	Copy->Tasks.Reset();
	for (UAKAbilityTaskBase* Task : Source->Tasks)
	{
		if (Task)
		{
			Copy->Tasks.Add(DuplicateObject<UAKAbilityTaskBase>(Task, Copy));
		}
	}

	AssetPtr->Tracks.Insert(Copy, Index + 1);
	AssetPtr->MarkPackageDirty();

	SelectedTrackIndex = Index + 1;
	PendingRenameIndex = Index + 1;
	NotifyChanged();
}

void SUAKTrackOutliner::RemoveTrack(int32 Index)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(Index) || IsTrackLocked(Index))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("RemoveTrack", "删除轨道"));
	AssetPtr->Modify();
	AssetPtr->Tracks.RemoveAt(Index);
	AssetPtr->MarkPackageDirty();

	if (SelectedTrackIndex == Index)
	{
		SelectedTrackIndex = INDEX_NONE;
	}
	else if (SelectedTrackIndex > Index)
	{
		--SelectedTrackIndex;
	}

	NotifyChanged();
}

void SUAKTrackOutliner::RenameTrack(int32 Index, const FString& NewName)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(Index) || IsTrackLocked(Index))
	{
		return;
	}

	UAKAbilityTrack* Track = AssetPtr->Tracks[Index];
	const FName NewFName = NewName.IsEmpty() ? NAME_None : FName(*NewName);
	if (Track->TrackName == NewFName)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("RenameTrack", "重命名轨道"));
	Track->Modify();
	Track->TrackName = NewFName;
	AssetPtr->MarkPackageDirty();

	NotifyChanged();
}

void SUAKTrackOutliner::ToggleMute(int32 Index)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(Index) || IsTrackLocked(Index))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ToggleMute", "轨道静音"));
	UAKAbilityTrack* Track = AssetPtr->Tracks[Index];
	Track->Modify();
	Track->bEnabled = !Track->bEnabled;
	AssetPtr->MarkPackageDirty();

	NotifyChanged();
}

void SUAKTrackOutliner::SetMuteForDrag(int32 Index, bool bInEnabled)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(Index) || IsTrackLocked(Index))
	{
		return;
	}

	UAKAbilityTrack* Track = AssetPtr->Tracks[Index];
	if (Track->bEnabled == bInEnabled)
	{
		return;
	}

	// 事务由拖拽操作持有（整段拖拽一个），这里只改数据
	Track->Modify();
	Track->bEnabled = bInEnabled;
	AssetPtr->MarkPackageDirty();

	// 拖拽中不能重建行（会打断拖拽），只让它们重绘
	for (const TWeakPtr<SUAKTrackRow>& Row : Rows)
	{
		if (TSharedPtr<SUAKTrackRow> RowPtr = Row.Pin())
		{
			RowPtr->Invalidate(EInvalidateWidgetReason::Paint);
		}
	}
}

void SUAKTrackOutliner::ToggleLock(int32 Index)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(Index))
	{
		return;
	}

	// 锁定本身不受锁定限制，否则锁上就再也开不开了
	const FScopedTransaction Transaction(LOCTEXT("ToggleLock", "轨道锁定"));
	UAKAbilityTrack* Track = AssetPtr->Tracks[Index];
	Track->Modify();
	Track->bLocked = !Track->bLocked;
	AssetPtr->MarkPackageDirty();

	NotifyChanged();
}

void SUAKTrackOutliner::ToggleCollapsed(int32 Index)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(Index))
	{
		return;
	}
	SetCollapsed(Index, !AssetPtr->Tracks[Index]->bCollapsed);
}

void SUAKTrackOutliner::SetCollapsed(int32 Index, bool bInCollapsed)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(Index))
	{
		return;
	}

	UAKAbilityTrack* Track = AssetPtr->Tracks[Index];
	if (Track->bCollapsed == bInCollapsed)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ToggleCollapse", "折叠轨道"));
	Track->Modify();
	Track->bCollapsed = bInCollapsed;
	AssetPtr->MarkPackageDirty();

	NotifyChanged();
}

TSharedRef<SWidget> SUAKTrackOutliner::GenerateTrackContextMenu(int32 Index)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	const bool bValid = AssetPtr && AssetPtr->Tracks.IsValidIndex(Index) && AssetPtr->Tracks[Index];
	const bool bLocked = IsTrackLocked(Index);

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	MenuBuilder.BeginSection(TEXT("Track"), LOCTEXT("MenuSection_Track", "轨道"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_Rename", "重命名"),
		LOCTEXT("Menu_RenameTip", "双击名字也可以改"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this, Index]()
			{
				SelectTrack(Index);
				RequestRenameForNextRefresh(Index);
				Refresh();
			}),
			FCanExecuteAction::CreateLambda([bValid, bLocked]() { return bValid && !bLocked; })));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_Duplicate", "复制轨道"),
		LOCTEXT("Menu_DuplicateTip", "连任务一起复制一份"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this, Index]() { DuplicateTrack(Index); }),
			FCanExecuteAction::CreateLambda([bValid, bLocked]() { return bValid && !bLocked; })));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_Remove", "删除轨道"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this, Index]() { RemoveTrack(Index); }),
			FCanExecuteAction::CreateLambda([bValid, bLocked]() { return bValid && !bLocked; })));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_AddBelow", "在下方新建轨道"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, Index]() { AddTrack(Index + 1); })));
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(TEXT("Order"), LOCTEXT("MenuSection_Order", "排列"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_Up", "上移"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, Index]() { MoveTrack(Index, -1); })));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_Down", "下移"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, Index]() { MoveTrack(Index, 1); })));
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(TEXT("State"), LOCTEXT("MenuSection_State", "状态"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_Mute", "静音"),
		LOCTEXT("Menu_MuteTip", "整条轨道不参与执行"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, Index]() { ToggleMute(Index); }),
			FCanExecuteAction::CreateLambda([bValid, bLocked]() { return bValid && !bLocked; })));
	// 没有"独奏"：技能不是多轨媒体，Solo 没有运行语义。要看单条轨道就把别的静音。
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_Lock", "锁定"),
		LOCTEXT("Menu_LockTip", "锁上后不能再改动这条轨道"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, Index]() { ToggleLock(Index); }),
			FCanExecuteAction::CreateLambda([bValid]() { return bValid; })));
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
