#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkitHost.h"

class UAKAbilityAsset;
class UAKAbilityTaskBase;
class SUAKTimeline;
class SUAKGASImpactBar;
class SUAKValidationBar;
class SUAKBloodlineView;
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

	void OnTaskSelected(UAKAbilityTaskBase* Task);
	void OnCursorTimeChanged(float Time);
	float GetCursorTime() const { return CursorTime; }

	TWeakObjectPtr<UAKAbilityAsset> Asset;
	TSharedPtr<SUAKTimeline> TimelineWidget;
	TSharedPtr<SUAKGASImpactBar> ImpactBarWidget;
	TSharedPtr<SUAKValidationBar> ValidationBarWidget;
	TSharedPtr<SUAKBloodlineView> BloodlineWidget;
	TSharedPtr<IDetailsView> DetailsView;

	float CursorTime = 0.f;

	static const FName TimelineTabId;
	static const FName DetailsTabId;
	static const FName BloodlineTabId;
};
