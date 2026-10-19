#include "SUAKTimelineToolbar.h"

#include "SUAKTimeline.h"

#include "SlateOptMacros.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SUAKTimelineToolbar"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SUAKTimelineToolbar::Construct(const FArguments& InArgs)
{
	Timeline = InArgs._Timeline;
	OnAction = InArgs._OnAction;

	ChildSlot
	[
		SNew(SHorizontalBox)

		// ── 播放控制 ──────────────────────────────────────────────────────
		+ SHorizontalBox::Slot().AutoWidth().Padding(1.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Stop", "回到起点"))
			.OnClicked(this, &SUAKTimelineToolbar::OnStopClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(1.f)
		[
			SNew(SButton)
			.Text(this, &SUAKTimelineToolbar::GetPlayButtonText)
			.OnClicked(this, &SUAKTimelineToolbar::OnPlayToggleClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(1.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("StepBack", "上一帧"))
			.OnClicked(this, &SUAKTimelineToolbar::OnStepBackClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(1.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("StepForward", "下一帧"))
			.OnClicked(this, &SUAKTimelineToolbar::OnStepForwardClicked)
		]

		// ── 显示 ──────────────────────────────────────────────────────────
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 1.f)
		[
			SNew(SCheckBox)
			.IsChecked(this, &SUAKTimelineToolbar::IsFrameModeChecked)
			.OnCheckStateChanged(this, &SUAKTimelineToolbar::OnFrameModeChanged)
			[
				SNew(STextBlock).Text(LOCTEXT("FrameMode", "按帧显示"))
			]
		]

		// ── 资产 ──────────────────────────────────────────────────────────
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 1.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("SelectAsset", "资产属性"))
			.ToolTipText(LOCTEXT("SelectAssetTip", "在 Inspector 里编辑资产本身（长度、Tags、Markers、中断顺序）"))
			.OnClicked(this, &SUAKTimelineToolbar::Dispatch, EAKToolbarAction::SelectAsset)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(1.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("AddMarker", "+ 标记"))
			.ToolTipText(LOCTEXT("AddMarkerTip", "在当前光标处添加一个命名时间点"))
			.OnClicked(this, &SUAKTimelineToolbar::Dispatch, EAKToolbarAction::AddMarker)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(1.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("GenFromMontage", "从 Montage 生成"))
			.ToolTipText(LOCTEXT("GenTip", "按预览 Montage 的长度设置技能长度，并把动画 Notify 转成标记"))
			.OnClicked(this, &SUAKTimelineToolbar::Dispatch, EAKToolbarAction::GenerateFromMontage)
		]

		// ── 轨道 ──────────────────────────────────────────────────────────
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 1.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("AddTrack", "+ 轨道"))
			.OnClicked(this, &SUAKTimelineToolbar::Dispatch, EAKToolbarAction::AddTrack)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(1.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("RemoveTrack", "- 轨道"))
			.OnClicked(this, &SUAKTimelineToolbar::Dispatch, EAKToolbarAction::RemoveTrack)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(1.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("TrackUp", "上移"))
			.OnClicked(this, &SUAKTimelineToolbar::Dispatch, EAKToolbarAction::MoveTrackUp)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(1.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("TrackDown", "下移"))
			.OnClicked(this, &SUAKTimelineToolbar::Dispatch, EAKToolbarAction::MoveTrackDown)
		]

		// ── 任务 ──────────────────────────────────────────────────────────
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 1.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("DupTask", "复制任务"))
			.ToolTipText(LOCTEXT("DupTaskTip", "复制当前选中的任务（Ctrl+D）"))
			.OnClicked(this, &SUAKTimelineToolbar::Dispatch, EAKToolbarAction::DuplicateTask)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(1.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("DelTask", "删除任务"))
			.ToolTipText(LOCTEXT("DelTaskTip", "删除当前选中的任务（Delete）"))
			.OnClicked(this, &SUAKTimelineToolbar::Dispatch, EAKToolbarAction::DeleteTask)
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText SUAKTimelineToolbar::GetPlayButtonText() const
{
	return Timeline.IsValid() && Timeline->IsPlaying()
		? LOCTEXT("Pause", "暂停")
		: LOCTEXT("Play", "播放");
}

FReply SUAKTimelineToolbar::OnPlayToggleClicked()
{
	if (Timeline.IsValid())
	{
		Timeline->TogglePlay();
	}
	return FReply::Handled();
}

FReply SUAKTimelineToolbar::OnStopClicked()
{
	if (Timeline.IsValid())
	{
		Timeline->Stop();
	}
	return FReply::Handled();
}

FReply SUAKTimelineToolbar::OnStepBackClicked()
{
	if (Timeline.IsValid())
	{
		Timeline->StepFrame(-1);
	}
	return FReply::Handled();
}

FReply SUAKTimelineToolbar::OnStepForwardClicked()
{
	if (Timeline.IsValid())
	{
		Timeline->StepFrame(1);
	}
	return FReply::Handled();
}

void SUAKTimelineToolbar::OnFrameModeChanged(ECheckBoxState NewState)
{
	if (Timeline.IsValid())
	{
		Timeline->SetShowFrames(NewState == ECheckBoxState::Checked);
	}
}

ECheckBoxState SUAKTimelineToolbar::IsFrameModeChecked() const
{
	return Timeline.IsValid() && Timeline->IsShowingFrames()
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

FReply SUAKTimelineToolbar::Dispatch(EAKToolbarAction Action)
{
	OnAction.ExecuteIfBound(Action);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
