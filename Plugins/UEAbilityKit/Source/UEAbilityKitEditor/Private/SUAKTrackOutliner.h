#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"   // FDragDropOperation / FDragDropEvent
#include "ScopedTransaction.h"
#include "UAKTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UAKAbilityAsset;
class UAKAbilityTrack;
class SScrollBox;
class SInlineEditableTextBlock;
class SUAKTrackOutliner;

/** 轨道类型色：左栏与时间轴共用一份，保证两边颜色永远一致 */
FLinearColor UAKColorForTrackType(EAKTrackType Type);

/**
 * 轨道行拖拽：带着源轨道下标走，落到目标行的上半区 / 下半区决定插到哪。
 * 抄 Sequencer 的 `FColumnToggleDragDropOp` 思路：一次拖拽只开一个事务。
 */
class FAKTrackDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FAKTrackDragDropOp, FDragDropOperation)

	int32 TrackIndex = INDEX_NONE;
	FText TrackLabel;

	static TSharedRef<FAKTrackDragDropOp> New(int32 InTrackIndex, FText InLabel);

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;
};

/**
 * 横拖批量刷静音：按住色条左右拖，扫过的轨道一起开/关。
 *
 * 抄 Sequencer 的 `FColumnToggleDragDropOp`：整段拖拽只开**一个**事务，
 * 刷过二十条轨道也只要撤销一次（否则每条一个事务，Ctrl+Z 会按到手酸）。
 */
class FAKTrackMuteDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FAKTrackMuteDragDropOp, FDragDropOperation)

	/** 扫过的轨道要变成的状态 */
	bool bTargetEnabled = false;

	/** 拖着的时候一直开着，抬手（析构）即提交 */
	TUniquePtr<FScopedTransaction> Transaction;

	static TSharedRef<FAKTrackMuteDragDropOp> New(bool bInTargetEnabled);
	virtual ~FAKTrackMuteDragDropOp();

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;
};

DECLARE_DELEGATE_TwoParams(FOnAKOutlinerTrackSelected, UAKAbilityTrack* /*Track*/, int32 /*TrackIndex*/);
DECLARE_DELEGATE(FOnAKOutlinerTracksChanged);
DECLARE_DELEGATE_OneParam(FOnAKOutlinerScrollChanged, float /*ScrollOffset*/);

class SUAKTrackOutliner;

/**
 * 左栏的一条轨道行。
 *
 * 元素从左到右：折叠 / 静音色条 / 类型标记 / 可就地重命名的名字 / 状态徽标 /
 * 右侧 Solo·Lock·删除（**hover 才淡入**，抄 `SColumnToggleWidget.cpp:103-150` 的五档透明度）。
 */
class SUAKTrackRow : public SCompoundWidget
{
public:
	/** 行首色条（静音开关）的宽度：在这个范围内按下并横向拖 = 批量刷静音 */
	static constexpr float MuteGutterWidth = 16.f;

	SLATE_BEGIN_ARGS(SUAKTrackRow) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UAKAbilityAsset>, Asset)
		SLATE_ARGUMENT(int32, TrackIndex)
		SLATE_ARGUMENT(TWeakPtr<SUAKTrackOutliner>, Outliner)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

	// SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	/** 键盘操作：上下键换行、F2 改名、Delete 删轨道（行要能拿到键盘焦点） */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;

	/** 新建后自动进入重命名（Persona 的 PendingRenameTrackIndex 做法） */
	void EnterRenameMode();

	bool IsHoveredRow() const { return bHovered; }
	int32 GetTrackIndex() const { return TrackIndex; }

private:
	FSlateColor GetRowBackgroundColor() const;
	/** 抄 Sequencer：激活 1.0 → 悬停按钮 0.8 → 行 hover 预览 0.25 → 无关 0.1 */
	FSlateColor GetToggleColor(const TSharedPtr<class SWidget>& Widget, bool bActive) const;
	FText GetTrackLabel() const;
	FText GetBadgeText() const;
	FText GetTypeGlyph() const;
	FSlateColor GetTypeColor() const;
	FSlateColor GetMuteColor() const;
	FText GetCollapseGlyph() const;
	void OnNameCommitted(const FText& NewText, ETextCommit::Type CommitType);

	bool IsLocked() const;

	TWeakObjectPtr<UAKAbilityAsset> Asset;
	TWeakPtr<SUAKTrackOutliner> Outliner;
	int32 TrackIndex = INDEX_NONE;

	TSharedPtr<SInlineEditableTextBlock> NameText;

	/** 右侧按钮：hover 淡入要读它们的 IsHovered()，所以得留住指针
	 *  （注意：不能在 SAssignNew 之前按值捕获到 lambda 里，那时还是空指针） */
	TSharedPtr<SWidget> LockButton;
	TSharedPtr<SWidget> DeleteButton;

	bool bHovered = false;
	bool bDropTarget = false;
	bool bDropAbove = false;
};

/**
 * 左栏：轨道列表（Outliner）。
 *
 * 骨架抄 Persona 的 `SAnimOutliner`（一根滚动条 + 固定行高 + 就地重命名），
 * 细节抄 Sequencer（hover 淡入、三态、拖放插入线、刷新分级）。
 *
 * 与右侧时间轴**共享滚动偏移**：左栏滚动 → 通知外部 → 时间轴按同一偏移画行，
 * 所以左右永远一行对一行（Sequencer 是行几何上报，我们直接用偏移量，代价更小）。
 */
class SUAKTrackOutliner : public SCompoundWidget
{
public:
	/** 行高：与时间轴共用，改一处即可 */
	static constexpr float RowHeight = 30.f;
	static constexpr float MinWidth = 120.f;

	SLATE_BEGIN_ARGS(SUAKTrackOutliner) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UAKAbilityAsset>, Asset)
		SLATE_EVENT(FOnAKOutlinerTrackSelected, OnTrackSelected)
		SLATE_EVENT(FOnAKOutlinerTracksChanged, OnTracksChanged)
		SLATE_EVENT(FOnAKOutlinerScrollChanged, OnScrollOffsetChanged)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

	// SWidget
	/** 空白处右键：新建轨道（行上的右键由行自己处理，不会冒到这里） */
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	void Refresh();

	void SetSelectedTrackIndex(int32 Index);
	int32 GetSelectedTrackIndex() const { return SelectedTrackIndex; }

	/** 外部（时间轴滚轮）请求滚动 */
	void ScrollBy(float DeltaPixels);
	/** 把第 Row 条轨道滚进视野（校验定位 / 血缘回跳时用） */
	void ScrollToRow(int32 Row);
	/** 只在行不在视野里时才滚（键盘上下换行用，避免每按一下都跳） */
	void EnsureRowVisible(int32 Row);
	float GetScrollOffset() const { return ScrollOffset; }

	// ── 轨道操作：统一在这里开事务，外部只管刷新 ──────────────────────────
	void SelectTrack(int32 Index);
	void ReorderTrack(int32 FromIndex, int32 ToIndex);
	void MoveTrack(int32 Index, int32 Delta);
	void AddTrack(int32 InsertIndex = INDEX_NONE);
	void DuplicateTrack(int32 Index);
	void RemoveTrack(int32 Index);
	void RenameTrack(int32 Index, const FString& NewName);
	void ToggleMute(int32 Index);
	/** 批量刷静音用：事务由拖拽操作持有，这里只改数据（拖拽中不能重建行，否则拖拽会断） */
	void SetMuteForDrag(int32 Index, bool bInEnabled);
	void ToggleLock(int32 Index);
	void ToggleCollapsed(int32 Index);
	void SetCollapsed(int32 Index, bool bInCollapsed);

	/** 轨道被锁定时禁止编辑 */
	bool IsTrackLocked(int32 Index) const;

	/** 下标是否在合法范围内（键盘上下换行用） */
	bool IsValidTrackIndex(int32 Index) const;
	/** 取某一行的控件（键盘焦点交接用） */
	TSharedPtr<SUAKTrackRow> GetRowWidget(int32 Index) const;

	/** 新建轨道后自动进入重命名（下一次 Refresh 生效） */
	void RequestRenameForNextRefresh(int32 Index) { PendingRenameIndex = Index; }

	/** 只通知外部刷新（时间轴 / 任务选择器），行自己不重建：拖拽收尾时用 */
	void NotifyTracksChangedExternal() { OnTracksChanged.ExecuteIfBound(); }

	TWeakObjectPtr<UAKAbilityAsset> GetAsset() const { return Asset; }

private:
	TSharedRef<SWidget> GenerateTrackContextMenu(int32 Index);
	void NotifyChanged();
	void HandleUserScrolled(float InScrollOffset);
	/** Construct 里不能 SharedThis，首帧再建行 */
	EActiveTimerReturnType InitialRefresh(double InCurrentTime, float InDeltaTime);

	TWeakObjectPtr<UAKAbilityAsset> Asset;
	FOnAKOutlinerTrackSelected OnTrackSelected;
	FOnAKOutlinerTracksChanged OnTracksChanged;
	FOnAKOutlinerScrollChanged OnScrollOffsetChanged;

	TSharedPtr<SScrollBox> ScrollBox;
	TSharedPtr<class SVerticalBox> RowsBox;
	/** 建出来的行：拖拽中只让它们重绘，不重建（重建会打断拖拽） */
	TArray<TWeakPtr<SUAKTrackRow>> Rows;

	int32 SelectedTrackIndex = INDEX_NONE;
	float ScrollOffset = 0.f;
	int32 PendingRenameIndex = INDEX_NONE;

	friend class SUAKTrackRow;
};
