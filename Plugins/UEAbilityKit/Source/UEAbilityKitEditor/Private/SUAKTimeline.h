#pragma once

#include "CoreMinimal.h"
#include "ScopedTransaction.h"
#include "SUAKTimelineToolbar.h"
#include "SUAKTrackOutliner.h"   // 左栏独立成控件后，行高与轨道色跟它共用一份
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
/** 批量删除：一次事务删掉选中的全部任务（多选后 Delete 用） */
DECLARE_DELEGATE_OneParam(FOnAKDeleteTasks, const TArray<UAKAbilityTaskBase*>& /*Tasks*/);
/** 裸滚轮转发成"滚动轨道列表"（交给左栏）：与 Sequencer 一致，Ctrl+滚轮才是缩放 */
DECLARE_DELEGATE_OneParam(FOnAKTimelineScrollBy, float /*DeltaPixels*/);
/** 把第 Row 条轨道滚进视野：校验定位、血缘回跳时，光水平 scroll 是不够的 */
DECLARE_DELEGATE_OneParam(FOnAKTimelineRevealRow, int32 /*Row*/);

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
	Pan,
	/** 在空白处拖出选框：批量选中 */
	Marquee
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
		SLATE_EVENT(FOnAKDeleteTasks, OnRequestDeleteTasks)
		SLATE_EVENT(FOnAKDuplicateTask, OnRequestDuplicateTask)
		SLATE_EVENT(FOnAKPasteTask, OnRequestPasteTask)
		SLATE_EVENT(FOnAKTimelineScrollBy, OnRequestScrollBy)
		SLATE_EVENT(FOnAKTimelineRevealRow, OnRequestRevealRow)
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
	bool IsLoopPlayback() const { return bLoopPlayback; }

	/** 缩放铺满整条技能（工具条"适应"）：内容再长也能一眼看全 */
	void ZoomToFit();

	// ── 吸附 ────────────────────────────────────────────────────────────
	/** 吸附总开关：关掉就是自由拖动（Sequencer 的 Snap 按钮同一语义） */
	bool IsSnapEnabled() const { return bSnapEnabled; }
	void SetSnapEnabled(bool bInEnabled);
	/** 是否吸附到命名标记（关掉则只按帧对齐） */
	bool IsSnapToMarkers() const { return bSnapToMarkers; }
	void SetSnapToMarkers(bool bInEnabled);

	/** 帧/秒显示切换 */
	void SetShowFrames(bool bInShowFrames) { bShowFrames = bInShowFrames; }
	bool IsShowingFrames() const { return bShowFrames; }

	int32 GetSelectedTrackIndex() const { return SelectedTrackIndex; }
	UAKAbilityTaskBase* GetSelectedTask() const { return SelectedTask.Get(); }
	/** 当前选中的全部任务（多选；单选时只有主选中的那一个） */
	const TArray<TWeakObjectPtr<UAKAbilityTaskBase>>& GetSelectedTasks() const { return SelectedTasks; }
	/** Ctrl+A：选中当前轨道（无选中轨道则全部轨道）上的所有任务 */
	void SelectAllTasks();
	/** 清空选中（编辑器删掉任务后调用，避免选中集合里留着已删除的对象） */
	void ClearSelection();
	float GetAssetLength() const;

	/** 让某个时间点进入视野（校验定位、血缘回跳、PIE 跟随用） */
	void ScrollTimeIntoView(float Time);

	/** 选中任务并把光标与视野带过去：一处实现，校验 / 血缘 / 搜索都能复用 */
	void FocusTask(UAKAbilityTaskBase* Task);

	/** 视图范围（视口左/右边缘对应的时间），工具条与外部联动用 */
	float GetViewStartTime() const { return ViewOffsetTime; }
	float GetViewEndTime() const;

	/**
	 * 与左栏共享的垂直滚动偏移（像素）。
	 *
	 * 抄 Sequencer/Persona：不做两个 ScrollBox 互相同步（会错帧），
	 * 而是两边共用同一个偏移量，左栏滚多少，这里就减多少。
	 */
	void SetScrollOffset(float InOffset) { ScrollOffset = InOffset; }
	float GetScrollOffset() const { return ScrollOffset; }

	/** 左栏选中轨道时同步过来：粘贴任务要知道落在哪条轨道 */
	void SetSelectedTrackIndex(int32 Index) { SelectedTrackIndex = Index; }

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
	/** 行顶端的 Y（已扣掉与左栏共享的滚动偏移） */
	float RowTopY(int32 Row) const;
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
	/** 这条轨道是否锁了（锁了就不能把任务挪进/挪出） */
	bool IsTrackLocked(int32 TrackIndex) const;
	/** 任务在第几条轨道上（右键"移到轨道"用） */
	int32 FindTrackRow(const UAKAbilityTaskBase* Task) const;
	FText GetHoverTooltipText() const;

	// ── 多选 ────────────────────────────────────────────────────────────
	/** 单选 / 加选（Ctrl 或 Shift）/ 减选，并同步主选中与 Inspector */
	void SetSelection(UAKAbilityTaskBase* Task, bool bAdditive);
	bool IsTaskSelected(const UAKAbilityTaskBase* Task) const;
	/** 框选：取出矩形范围内的任务（用执行计划的真实时间判定） */
	void CollectTasksInRect(const FVector2D& Start, const FVector2D& End, TArray<UAKAbilityTaskBase*>& OutTasks) const;
	/** 批量移动：整组保持相对时间一起挪 */
	void MoveSelectionBy(float DeltaTime);
	/** 删除当前选中：多选走批量删除委托，单选走单任务委托 */
	void DeleteSelection();

	void DrawRuler(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void DrawTrackRows(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void DrawSegments(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void DrawCursor(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void DrawMarquee(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

private:
	TWeakObjectPtr<UAKAbilityAsset> Asset;
	FOnAKTaskSelected OnTaskSelected;
	FOnAKTrackSelected OnTrackSelected;
	FOnAKCursorTimeChanged OnCursorTimeChanged;
	FOnAKTimelineRequest OnRequestDeleteTask;
	FOnAKDeleteTasks OnRequestDeleteTasks;
	FOnAKDuplicateTask OnRequestDuplicateTask;
	FOnAKPasteTask OnRequestPasteTask;
	FOnAKTimelineScrollBy OnRequestScrollBy;
	FOnAKTimelineRevealRow OnRequestRevealRow;
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

	/** 吸附开关（存在 UUAKSettings 里，下次打开还是这个选择） */
	bool bSnapEnabled = true;
	bool bSnapToMarkers = true;

	/** 一次性教学提示：首次拖动时告诉用户"按 Shift 可以让播放头跟着走" */
	double ScrubHintStartTime = -1.0;
	double LastTickTime = 0.0;

	/** 与左栏共享的垂直滚动偏移（像素）：行画在哪、命中哪一行的基准 */
	float ScrollOffset = 0.f;

	/** 拖拽状态：一次按下只做一件事，抬起时提交事务 */
	EAKTimelineDragMode DragMode = EAKTimelineDragMode::None;
	TWeakObjectPtr<UAKAbilityTaskBase> DraggingTask;
	float DragGrabOffset = 0.f;
	float DragStartLocalX = 0.f;
	float DragStartViewOffset = 0.f;
	int32 DraggingMarkerIndex = INDEX_NONE;
	int32 DragOriginTrackIndex = INDEX_NONE;
	int32 DragCurrentTrackIndex = INDEX_NONE;
	/** 被拖任务按下时的起点：批量移动据此算整组的位移 */
	float DragOriginTaskStart = 0.f;
	/** Alt+拖拽：真正开始移动后才复制，避免 Alt+单击凭空多一份 */
	bool bAltPendingDuplicate = false;

	/** 框选：起点（局部坐标）与当前点；拖动中实时画矩形 */
	FVector2D MarqueeStart = FVector2D::ZeroVector;
	FVector2D MarqueeCurrent = FVector2D::ZeroVector;
	/** 框选时按住 Ctrl / Shift = 在已有选中上追加 */
	bool bMarqueeAdditive = false;

	/** 批量移动时每个任务的原起点，保证整组相对关系不变 */
	TArray<TPair<TWeakObjectPtr<UAKAbilityTaskBase>, float>> MoveSelectionStarts;

	/** 一次拖动 = 一个事务（抬起鼠标时提交） */
	TUniquePtr<FScopedTransaction> DragTransaction;

	TWeakObjectPtr<UAKAbilityTaskBase> SelectedTask;
	/** 多选集合；SelectedTask 是其中"主选中"的那一个（Inspector 看它） */
	TArray<TWeakObjectPtr<UAKAbilityTaskBase>> SelectedTasks;
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

	/** 左栏已拆成独立控件（SUAKTrackOutliner），时间轴只画右侧，所以没有表头宽度 */
	static constexpr float TrackHeaderWidth = 0.f;
	static constexpr float RulerHeight = 26.f;
	/** 行高与左栏共用：改一处，两边永远对齐 */
	static constexpr float RowHeight = SUAKTrackOutliner::RowHeight;
};
