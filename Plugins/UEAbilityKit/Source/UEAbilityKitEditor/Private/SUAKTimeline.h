#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UAKAbilityAsset;
class UAKAbilityTaskBase;

class UAKAbilityTrack;

DECLARE_DELEGATE_OneParam(FOnAKTaskSelected, UAKAbilityTaskBase* /*Task*/);
DECLARE_DELEGATE_TwoParams(FOnAKTrackSelected, UAKAbilityTrack* /*Track*/, int32 /*TrackIndex*/);
DECLARE_DELEGATE_OneParam(FOnAKCursorTimeChanged, float /*Time*/);
DECLARE_DELEGATE(FOnAKTimelineRequest);

/**
 * 时间轴画布（自绘 Slate）。
 *
 * 布局刻意与 Able 不同：
 * - 左栏是「轨道头 + 整体开关」，而不是 Ability 属性面板；
 * - 视口不是常驻区域，而是独立 Tab，因此时间轴可以占满主工作区；
 * - 校验结果不弹窗，而是常驻底栏（见 SUAKValidationBar）。
 *
 * 通用交互（拖拽 / 缩放 / 吸附 / 单帧步进）属于业界通用做法，与其他编辑器保持一致。
 */
class SUAKTimeline : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUAKTimeline) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UAKAbilityAsset>, Asset)
		SLATE_EVENT(FOnAKTaskSelected, OnTaskSelected)
		SLATE_EVENT(FOnAKTrackSelected, OnTrackSelected)
		SLATE_EVENT(FOnAKCursorTimeChanged, OnCursorTimeChanged)
		SLATE_EVENT(FOnAKTimelineRequest, OnRequestDeleteTask)
		SLATE_EVENT(FOnAKTimelineRequest, OnRequestDuplicateTask)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	float GetCursorTime() const { return CursorTime; }
	void SetCursorTime(float InTime);
	void StepFrame(int32 Frames);

	/** 播放控制 */
	void Play();
	void Pause();
	void TogglePlay();
	void Stop();
	bool IsPlaying() const { return bPlaying; }
	void SetLoopPlayback(bool bInLoop) { bLoopPlayback = bInLoop; }

	/** 帧/秒显示切换 */
	void SetShowFrames(bool bInShowFrames) { bShowFrames = bInShowFrames; }
	bool IsShowingFrames() const { return bShowFrames; }

	int32 GetSelectedTrackIndex() const { return SelectedTrackIndex; }
	UAKAbilityTaskBase* GetSelectedTask() const { return SelectedTask.Get(); }
	float GetAssetLength() const;

private:
	float TimeToX(float Time) const;
	float XToTime(float X) const;
	int32 HitTestRow(float Y) const;
	UAKAbilityTaskBase* HitTestTask(float X, int32 Row) const;
	float SnapTime(float Time) const;
	void RebuildPlanIfNeeded();

	void DrawRuler(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void DrawTrackRows(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void DrawSegments(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void DrawCursor(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

private:
	TWeakObjectPtr<UAKAbilityAsset> Asset;
	FOnAKTaskSelected OnTaskSelected;
	FOnAKTrackSelected OnTrackSelected;
	FOnAKCursorTimeChanged OnCursorTimeChanged;

	/**
	 * 缓存的执行计划：复合轨道（Sequence / Parallel / Repeat）展开后的真实时间。
	 * 编辑器据此绘制，保证"看到的"就是"跑出来的"。
	 */
	TArray<FAKScheduledTask> CachedPlan;
	float PlanRefreshTimer = 0.f;
	FString LastPlanSignature;

	mutable float ViewWidth = 0.f;
	mutable float ViewHeight = 0.f;

	float CursorTime = 0.f;
	float PixelsPerSecond = 220.f;
	bool bShowFrames = false;

	/** 拖拽状态 */
	TWeakObjectPtr<UAKAbilityTaskBase> DraggingTask;
	float DragGrabOffset = 0.f;
	bool bDragging = false;

	TWeakObjectPtr<UAKAbilityTaskBase> SelectedTask;
	int32 SelectedTrackIndex = INDEX_NONE;

	/** 播放状态 */
	bool bPlaying = false;
	bool bLoopPlayback = false;
	float PlaybackRate = 1.f;

	static constexpr float TrackHeaderWidth = 140.f;
	static constexpr float RulerHeight = 26.f;
	static constexpr float RowHeight = 30.f;
};
