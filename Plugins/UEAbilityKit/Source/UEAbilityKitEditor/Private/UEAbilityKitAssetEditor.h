#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "SUAKTimelineToolbar.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkitHost.h"

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
class FUEAbilityKitAssetEditor : public FAssetEditorToolkit
{
public:
	void InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UAKAbilityAsset* InAsset);
	virtual ~FUEAbilityKitAssetEditor();

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

	void OnTaskSelected(UAKAbilityTaskBase* Task);
	void OnTrackSelected(class UAKAbilityTrack* Track, int32 TrackIndex);
	void OnCursorTimeChanged(float Time);
	float GetCursorTime() const { return CursorTime; }

	/** 键盘 / 工具条：删除与复制当前选中的任务 */
	void DeleteSelectedTask();
	void DuplicateSelectedTask();

	/** 任务选择器：把新任务加到指定轨道（INDEX_NONE 表示新建轨道） */
	void OnAddTask(TSubclassOf<UAKAbilityTaskBase> TaskClass, int32 TrackIndex);

	/** 工具条：改资产的操作 */
	void OnToolbarAction(EAKToolbarAction Action);
	void AddTrack();
	void RemoveSelectedTrack();
	void MoveSelectedTrack(int32 Delta);
	void AddMarkerAtCursor();
	void SelectAssetInInspector();
	void GenerateFromMontage();

	/** 预览视口工具条 */
	FReply OnPreviewPlayClicked();
	FReply OnPreviewStopClicked();
	FReply OnPreviewLoopClicked();

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

	static const FName TimelineTabId;
	static const FName DetailsTabId;
	static const FName BloodlineTabId;
	static const FName PreviewTabId;
};
