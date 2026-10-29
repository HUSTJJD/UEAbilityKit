#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SUAKTimeline;

/** 工具条上会改变资产的操作 */
enum class EAKToolbarAction : uint8
{
	AddTrack,
	RemoveTrack,
	MoveTrackUp,
	MoveTrackDown,
	DuplicateTask,
	DeleteTask,
	AddMarker,
	RemoveMarker,
	SelectAsset,
	GenerateFromMontage
};

DECLARE_DELEGATE_OneParam(FOnAKToolbarAction, EAKToolbarAction);

/**
 * 时间轴工具条。
 *
 * 分组：播放控制 / 显示 / 资产 / 轨道 / 任务 / 标记 / 从 Montage 生成。
 * 播放控制直接作用于时间轴，改资产的操作通过委托交回编辑器（编辑器持有事务与撤销上下文）。
 */
class SUAKTimelineToolbar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUAKTimelineToolbar) {}
		SLATE_ARGUMENT(TSharedPtr<SUAKTimeline>, Timeline)
		SLATE_EVENT(FOnAKToolbarAction, OnAction)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnPlayToggleClicked();
	FReply OnStopClicked();
	FReply OnStepBackClicked();
	FReply OnStepForwardClicked();
	/** 缩放铺满整条技能 */
	FReply OnFitClicked();
	/** 吸附总开关：关掉后可自由拖动（Sequencer 的 Snap 按钮同一语义） */
	void OnSnapChanged(ECheckBoxState NewState);
	ECheckBoxState IsSnapChecked() const;
	/** 循环播放：以前只有一个没人调用的 SetLoopPlayback，工具条上没有入口 */
	void OnLoopChanged(ECheckBoxState NewState);
	ECheckBoxState IsLoopChecked() const;
	FText GetPlayButtonText() const;
	void OnFrameModeChanged(ECheckBoxState NewState);
	ECheckBoxState IsFrameModeChecked() const;

	FReply Dispatch(EAKToolbarAction Action);

	TSharedPtr<SUAKTimeline> Timeline;
	FOnAKToolbarAction OnAction;
};
