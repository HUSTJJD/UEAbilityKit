#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "EditorUndoClient.h"
#include "SUAKTimelineToolbar.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkitHost.h"
#include "UAKTutorialTypes.h"   // EUAKTutorialRegion（教程自动化采哪个区域）
#include "Widgets/SWidget.h"

class UAKAbilityAsset;
class UAKAbilityTaskBase;
class UAKTrace;
class SUAKTimeline;
class SUAKGASImpactBar;
class SUAKValidationBar;
class SUAKBloodlineView;
class SUAKEditorViewport;
class SUAKTaskPicker;
class IDetailsView;

/**
 * 技能资产编辑器。
 *
 * 三个 Tab（而非 Able 的"Timeline + 蓝图图"双模式）：
 * - Timeline：时间轴编排（内含 GAS 影响条与常驻校验条）
 * - Bloodline：血缘 / Explain 视图（UAK 独创）
 * - Details：Inspector（分层展示当前选中任务的参数）
 */
class FUEAbilityKitAssetEditor : public FAssetEditorToolkit, public FEditorUndoClient
{
public:
	void InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UAKAbilityAsset* InAsset);
	virtual ~FUEAbilityKitAssetEditor();

	// ─────────────────────────────────────────────────────────────────────
	// 教程自动化桥接（UAK.Tutorial.*）：让教程引擎能"开着编辑器亲手操作一遍"
	//
	// 为什么要这一层：教程要截的是面板（时间轴 / 血缘 / Inspector），
	// 而不是视口；而驱动 UI 又必须走真实的控件，否则"录出来的"和"读者点的"是两回事。
	// 这一层只做转发，不含业务逻辑。
	// ─────────────────────────────────────────────────────────────────────

	/** 当前打开的所有技能资产编辑器（教程引擎据此找到要操作的那一台） */
	static TSharedPtr<FUEAbilityKitAssetEditor> FindEditorForAsset(const UAKAbilityAsset* InAsset);
	/** 按名字取 TabId：Timeline / Bloodline / Details / Preview */
	static FName GetTabIdForName(const FString& Name);

	/**
	 * 教程要截哪个区域；拿不到就返回空（例如 Tab 还没被打开过）
	 *
	 * 不是 const：要查 Tab 内容得走 FAssetEditorToolkit::GetTabManager()，
	 * 而引擎那个访问器是非 const 的（它会返回可写的 TabManager）。
	 */
	TSharedPtr<SWidget> GetWidgetForRegion(EUAKTutorialRegion Region);

	/**
	 * 给面板里的关键控件打上 Slate Tag（UAK.Timeline / UAK.Bloodline / …）。
	 *
	 * 官方 IntroTutorials 的 NamedWidget 锚点匹配的就是 `SWidget::GetTag()`
	 * ——这是 Slate 通用机制，所以 UAK 自己的面板打了 Tag 就能被引导的高亮框住，
	 * 不需要引擎那套内部 wrapper。
	 */
	static void ApplyTutorialTags(const TSharedPtr<SWidget>& Widget, EUAKTutorialRegion Region);

	void AutomationFocusTab(FName TabId);
	void AutomationRefresh();
	void AutomationSetCursorTime(float InTime);
	void AutomationPlay();
	void AutomationPause();
	void AutomationStop();
	void AutomationStepFrames(int32 Frames);
	void AutomationSetShowFrames(bool bInShowFrames);
	void AutomationSelectTask(UAKAbilityTaskBase* Task);
	void AutomationSelectTrack(int32 TrackIndex);
	float GetCursorTimePublic() const { return GetCursorTime(); }

	/**
	 * 引导（UAK.Tutorial.Guided）用的进度快照。
	 *
	 * 与上面那些"执行"接口不同，这一组是**只读**的：引导模式下是读者在动手，
	 * 引擎只负责判断"目标状态达成没有"，所以只需要把当前状态取出来比对。
	 */
	struct FGuidedStats
	{
		int32 Tracks = 0;
		int32 Tasks = 0;
		float CursorTime = 0.f;
		bool bHasSelectedTask = false;
		bool bHasSelectedTrack = false;
		int32 Errors = 0;
		int32 Warnings = 0;
	};

	/** @param bNeedValidation 只有脚本声明了 expectErrors 时才跑校验，避免每 tick 白算 */
	FGuidedStats GetGuidedStats(bool bNeedValidation = false) const;

	// FAssetEditorToolkit
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

private:
	TSharedRef<SDockTab> SpawnTab_Timeline(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Bloodline(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Preview(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Validation(const FSpawnTabArgs& Args);

	void OnTaskSelected(UAKAbilityTaskBase* Task);
	void OnTrackSelected(class UAKAbilityTrack* Track, int32 TrackIndex);
	void OnCursorTimeChanged(float Time);
	float GetCursorTime() const { return CursorTime; }

	/**
	 * 把某个任务带到眼前：选中它 → 光标落到它的起点 → 把它滚进视野 → 把时间轴 Tab 翻出来。
	 * 校验双击、血缘双击、GAS 影响条下钻都走这一条路，行为才一致。
	 * @param Task 为空表示资产级问题（没有可跳的片段），只把 Inspector 收回资产。
	 */
	void RevealTaskInTimeline(UAKAbilityTaskBase* Task);

	/** 影响条下钻：除了定位到时间轴，还把血缘视图翻出来 */
	void RevealTaskInBloodline(UAKAbilityTaskBase* Task);

	/** 键盘 / 工具条：删除与复制当前选中的任务（复制返回新任务，供 Alt+拖拽接管） */
	void DeleteSelectedTask();
	/**
	 * @param bMergeIntoActiveTransaction 时间轴拖动已经开好事务时传 true，
	 *        让"复制 + 移动"留在同一个撤销步里。
	 */
	UAKAbilityTaskBase* DuplicateSelectedTask(bool bMergeIntoActiveTransaction = false);

	/** 粘贴：把时间轴剪贴板里的 Json 还原成任务，落在当前轨道的光标处 */
	UAKAbilityTaskBase* PasteTaskFromJson(const FString& TaskJson);

	// FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	/** 撤销 / 重做后刷新所有视图 */
	void RefreshAfterUndo();

	/** 任务选择器：把新任务加到指定轨道（INDEX_NONE 表示新建轨道） */
	void OnAddTask(TSubclassOf<UAKAbilityTaskBase> TaskClass, int32 TrackIndex);

	/** 工具条：改资产的操作 */
	void OnToolbarAction(EAKToolbarAction Action);
	void AddTrack();
	void RemoveSelectedTrack();
	void MoveSelectedTrack(int32 Delta);
	void AddMarkerAtCursor();
	/** 删掉时间轴右键点中的那个标记（INDEX_NONE / 越界则不动） */
	void RemoveMarkerAt(int32 MarkerIndex);
	void SelectAssetInInspector();
	void GenerateFromMontage();

	/** 预览视口工具条 */
	FReply OnPreviewPlayClicked();
	FReply OnPreviewStopClicked();
	FReply OnPreviewLoopClicked();

	/** 预览角色下拉（转发给预览视口） */
	TSharedRef<SWidget> MakePreviewCharacterMenu();

	/** 顶部工具条（Save 那一行）：播放 / 停止 / 循环 / 角色 */
	void FillToolbar(class FToolBarBuilder& Builder);

	/** Inspector 顶部的面包屑：当前在编辑资产还是某个任务 */
	FText GetInspectedObjectLabel() const;

	/** PIE 联动：光标跟随 + 血缘视图切实况数据 */
	bool TickPIELink(float DeltaTime);

	TWeakObjectPtr<UAKAbilityAsset> Asset;
	TSharedPtr<SUAKTimeline> TimelineWidget;
	TSharedPtr<SUAKGASImpactBar> ImpactBarWidget;
	TSharedPtr<SUAKValidationBar> ValidationBarWidget;
	TSharedPtr<SUAKBloodlineView> BloodlineWidget;
	TSharedPtr<SUAKEditorViewport> PreviewWidget;
	TSharedPtr<SUAKTaskPicker> TaskPickerWidget;
	TSharedPtr<IDetailsView> DetailsView;

	float CursorTime = 0.f;

	/** 当前在 Inspector 里编辑的对象 */
	TWeakObjectPtr<UAKAbilityTaskBase> SelectedTask;
	int32 SelectedTrackIndex = INDEX_NONE;

	FTSTicker::FDelegateHandle PIETickerHandle;
	TWeakObjectPtr<const UAKTrace> LastLinkedTrace;
	bool bPIERecordingForced = false;

	/** 所有活着的编辑器实例：教程自动化靠它"找到"要驱动的那台 */
	static TArray<TWeakPtr<FUEAbilityKitAssetEditor>> OpenEditors;

public:
	static const FName TimelineTabId;
	static const FName DetailsTabId;
	static const FName BloodlineTabId;
	static const FName PreviewTabId;
	static const FName ValidationTabId;
};
