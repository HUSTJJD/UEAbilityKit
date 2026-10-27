#pragma once

#include "CoreMinimal.h"
#include "ScopedTransaction.h"
#include "SUAKTimelineToolbar.h"
#include "UAKTypes.h"   // FAKScheduledTask（执行计划缓存）
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UAKAbilityAsset;
class UAKAbilityTaskBase;

class UAKAbilityTrack;

DECLARE_DELEGATE_OneParam(FOnAKTaskSelected, UAKAbilityTaskBase* /*Task*/);
DECLARE_DELEGATE_RetVal_OneParam(UAKAbilityTaskBase*, FOnAKPasteTask, const FString& /*TaskJson*/);
DECLARE_DELEGATE_TwoParams(FOnAKTrackSelected, UAKAbilityTrack* /*Track*/, int32 /*TrackIndex*/);
DECLARE_DELEGATE_OneParam(FOnAKCursorTimeChanged, float /*Time*/);
DECLARE_DELEGATE(FOnAKTimelineRequest);
/**
 * 复制当前选中任务。
 * @param bMergeIntoActiveTransaction 调用方（时间轴拖动）已经开好事务时为 true，
 *        此时不再另开事务——否则 Alt+拖拽一次操作会留下两个撤销步。
 */
DECLARE_DELEGATE_RetVal_OneParam(UAKAbilityTaskBase*, FOnAKDuplicateTask, bool /*bMergeIntoActiveTransaction*/);

/** 时间轴上正在进行的拖拽种类：决定鼠标移动时改什么 */
enum class EAKTimelineDragMode : uint8
{
	None,
	/** 按住左键左右拖：连续 scrub 光标 */
	Scrub,
	/** 拖动整个片段 */
	Move,
	/** 拖左端：改 StartTime 与 Duration（右端不动） */
	TrimLeft,
	/** 拖右端：改 Duration */
	TrimRight,
	/** 拖动命名标记的时间 */
	Marker,
	/** 中键 / Shift+滚轮：平移视图 */
	Pan
};

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
		SLATE_EVENT(FOnAKDuplicateTask, OnRequestDuplicateTask)
		SLATE_EVENT(FOnAKPasteTask, OnRequestPasteTask)
		SLATE_EVENT(FOnAKToolbarAction, OnAction)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
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

	/** 让某个时间点进入视野（校验定位、血缘回跳、PIE 跟随用） */
	void ScrollTimeIntoView(float Time);

	/** 选中任务并把光标与视野带过去：一处实现，校验 / 血缘 / 搜索都能复用 */
	void FocusTask(UAKAbilityTaskBase* Task);

	/** 视图范围（视口左/右边缘对应的时间），工具条与外部联动用 */
	float GetViewStartTime() const { return ViewOffsetTime; }
	float GetViewEndTime() const;

	/** 右键菜单命中的标记下标（INDEX_NONE = 没命中）；编辑器据此删除标记 */
	int32 GetContextMarkerIndex() const { return ContextMarkerIndex; }
	void ClearContextMarkerIndex() { ContextMarkerIndex = INDEX_NONE; }

	/** 外部改动资产后强制重建执行计划（撤销/重做用） */
	void ForceRefresh();

	/** 复制 / 粘贴：Ctrl+C 把选中任务存成 Json，Ctrl+V 粘到光标处 */
	void CopySelectedTask();
	void PasteTask();
	bool HasCopiedTask() const { return !TaskClipboard.IsEmpty(); }

private:
	/** 右键菜单 */
	TSharedRef<SWidget> GenerateContextMenu(UAKAbilityTaskBase* HitTask, int32 Row);
	void ShowContextMenu(const FPointerEvent& MouseEvent, const FGeometry& MyGeometry, UAKAbilityTaskBase* HitTask, int32 Row);

private:
	float TimeToX(float Time) const;
	float XToTime(float X) const;
	int32 HitTestRow(float Y) const;
	UAKAbilityTaskBase* HitTestTask(float X, int32 Row) const;
	/** 命中命名标记（±5px 内），返回下标 */
	int32 HitTestMarker(float X) const;
	/**
	 * 判定片段边缘：左右 6px（短片段按比例收窄）内算"拉伸"，否则算"整体移动"。
	 * @param OutScheduled 命中片段对应的执行计划条目（复合轨道展开后的真实时间）
	 */
	EAKTimelineDragMode HitTestSegmentEdge(float X, int32 Row, UAKAbilityTaskBase*& OutTask, const FAKScheduledTask*& OutScheduled) const;
	float SnapTime(float Time) const;
	void RebuildPlanIfNeeded();

	/** 视图偏移的合法范围：不允许拖到 0 之前，也不允许把内容全推出视口 */
	void ClampViewOffset();
	/** 把任务从一条轨道挪到另一条（跨轨道拖拽） */
	void MoveTaskToTrack(UAKAbilityTaskBase* Task, int32 FromTrackIndex, int32 ToTrackIndex);
	/** 拉伸区间：复合轨道（时间由轨道算）不允许手动改 */
	bool CanEditTaskTimes(const UAKAbilityTaskBase* Task) const;
	FText GetHoverTooltipText() const;

	void DrawRuler(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void DrawTrackRows(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void DrawSegments(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void DrawCursor(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

private:
	TWeakObjectPtr<UAKAbilityAsset> Asset;
	FOnAKTaskSelected OnTaskSelected;
	FOnAKTrackSelected OnTrackSelected;
	FOnAKCursorTimeChanged OnCursorTimeChanged;
	FOnAKTimelineRequest OnRequestDeleteTask;
	FOnAKDuplicateTask OnRequestDuplicateTask;
	FOnAKPasteTask OnRequestPasteTask;
	FOnAKToolbarAction OnAction;

	/** 任务剪贴板：存 Json 文本而不是 UObject 指针，撤销/重做后依然有效 */
	FString TaskClipboard;

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

	/** 视口左边缘对应的时间：横向平移与缩放锚点都靠它 */
	float ViewOffsetTime = 0.f;

	/** 拖拽状态：一次按下只做一件事，抬起时提交事务 */
	EAKTimelineDragMode DragMode = EAKTimelineDragMode::None;
	TWeakObjectPtr<UAKAbilityTaskBase> DraggingTask;
	float DragGrabOffset = 0.f;
	float DragStartLocalX = 0.f;
	float DragStartViewOffset = 0.f;
	int32 DraggingMarkerIndex = INDEX_NONE;
	int32 DragOriginTrackIndex = INDEX_NONE;
	int32 DragCurrentTrackIndex = INDEX_NONE;
	/** Alt+拖拽：真正开始移动后才复制，避免 Alt+单击凭空多一份 */
	bool bAltPendingDuplicate = false;

	/** 一次拖动 = 一个事务（抬起鼠标时提交） */
	TUniquePtr<FScopedTransaction> DragTransaction;

	TWeakObjectPtr<UAKAbilityTaskBase> SelectedTask;
	int32 SelectedTrackIndex = INDEX_NONE;

	/** 悬停状态：高亮片段、显示拉伸把手、tooltip 都读它 */
	FVector2D HoverLocalPos = FVector2D::ZeroVector;
	TWeakObjectPtr<UAKAbilityTaskBase> HoverTask;
	int32 HoverRow = INDEX_NONE;
	int32 HoverMarkerIndex = INDEX_NONE;
	int32 ContextMarkerIndex = INDEX_NONE;

	/** 播放状态 */
	bool bPlaying = false;
	bool bLoopPlayback = false;
	float PlaybackRate = 1.f;

	static constexpr float TrackHeaderWidth = 140.f;
	static constexpr float RulerHeight = 26.f;
	static constexpr float RowHeight = 30.f;
};
