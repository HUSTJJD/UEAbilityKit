#include "UEAbilityKitAssetEditor.h"

#include "SUAKBloodlineView.h"
#include "SUAKGASImpactBar.h"
#include "SUAKTimeline.h"
#include "SUAKValidationBar.h"
#include "UAKAbilityAsset.h"
#include "UAKAbilityTaskBase.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FUEAbilityKitAssetEditor"

const FName FUEAbilityKitAssetEditor::TimelineTabId(TEXT("UAKTimeline"));
const FName FUEAbilityKitAssetEditor::DetailsTabId(TEXT("UAKDetails"));
const FName FUEAbilityKitAssetEditor::BloodlineTabId(TEXT("UAKBloodline"));

void FUEAbilityKitAssetEditor::InitEditor(const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost, UAKAbilityAsset* InAsset)
{
	Asset = InAsset;

	// Inspector：分层展示（时间 / 目标 / 效果 / 表现）——由 DetailsView 的默认分类呈现
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyModule.CreateDetailView(DetailsArgs);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("UAKAbilityEditor_Layout_v1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.72f)
				->Split
				(
					FTabManager::NewStack()->SetSizeCoefficient(0.75f)
					->AddTab(TimelineTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()->SetSizeCoefficient(0.25f)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
				)
			)
			->Split
			(
				FTabManager::NewStack()->SetSizeCoefficient(0.28f)
				->AddTab(BloodlineTabId, ETabState::OpenedTab)
			)
		);

	InitAssetEditor(Mode, InitToolkitHost, FName(TEXT("UAKAbilityEditorApp")), Layout,
		/*bCreateDefaultStandaloneMenu=*/true, /*bCreateDefaultToolbar=*/true, InAsset);
}

void FUEAbilityKitAssetEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenu::GetMenuStructure()->GetStructureBuilder()
		->AddCategory(LOCTEXT("UAKCategory", "UE Ability Kit"), FSlateIcon(), 1000);

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(TimelineTabId,
		FOnSpawnTab::CreateRaw(this, &FUEAbilityKitAssetEditor::SpawnTab_Timeline))
		.SetDisplayName(LOCTEXT("TimelineTab", "时间轴"))
		.SetGroup(WorkspaceMenu::GetMenuStructure()->GetAssetEditorCategory());

	InTabManager->RegisterTabSpawner(DetailsTabId,
		FOnSpawnTab::CreateRaw(this, &FUEAbilityKitAssetEditor::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTab", "Inspector"))
		.SetGroup(WorkspaceMenu::GetMenuStructure()->GetAssetEditorCategory());

	InTabManager->RegisterTabSpawner(BloodlineTabId,
		FOnSpawnTab::CreateRaw(this, &FUEAbilityKitAssetEditor::SpawnTab_Bloodline))
		.SetDisplayName(LOCTEXT("BloodlineTab", "血缘"))
		.SetGroup(WorkspaceMenu::GetMenuStructure()->GetAssetEditorCategory());
}

void FUEAbilityKitAssetEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(TimelineTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(BloodlineTabId);
}

FName FUEAbilityKitAssetEditor::GetToolkitFName() const { return FName(TEXT("UEAbilityKitAssetEditor")); }

FText FUEAbilityKitAssetEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "UE Ability Kit 技能编辑器");
}

FString FUEAbilityKitAssetEditor::GetWorldCentricTabPrefix() const { return TEXT("UAK "); }

FLinearColor FUEAbilityKitAssetEditor::GetWorldCentricTabColorScale() const { return FLinearColor(0.38f, 0.85f, 0.98f); }

TSharedRef<SDockTab> FUEAbilityKitAssetEditor::SpawnTab_Timeline(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == TimelineTabId);

	TSharedRef<SUAKTimeline> Timeline = SNew(SUAKTimeline)
		.Asset(Asset)
		.OnTaskSelected(this, &FUEAbilityKitAssetEditor::OnTaskSelected)
		.OnCursorTimeChanged(this, &FUEAbilityKitAssetEditor::OnCursorTimeChanged);
	TimelineWidget = Timeline;

	TSharedRef<SUAKGASImpactBar> ImpactBar = SNew(SUAKGASImpactBar)
		.Asset(Asset)
		.CursorTime(this, &FUEAbilityKitAssetEditor::GetCursorTime);
	ImpactBarWidget = ImpactBar;

	ValidationBarWidget = SNew(SUAKValidationBar).Asset(Asset);

	return SNew(SDockTab)
		.Label(LOCTEXT("TimelineTabLabel", "时间轴"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				Timeline
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder).Padding(4.f)
				[
					ImpactBar
				]
			]
			+ SVerticalBox::Slot().AutoHeight().MaxHeight(140.f)
			[
				SNew(SBorder).Padding(4.f)
				[
					ValidationBarWidget.ToSharedRef()
				]
			]
		];
}

TSharedRef<SDockTab> FUEAbilityKitAssetEditor::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == DetailsTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTabLabel", "Inspector"))
		[
			DetailsView.ToSharedRef()
		];
}

TSharedRef<SDockTab> FUEAbilityKitAssetEditor::SpawnTab_Bloodline(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == BloodlineTabId);

	BloodlineWidget = SNew(SUAKBloodlineView).Asset(Asset);

	return SNew(SDockTab)
		.Label(LOCTEXT("BloodlineTabLabel", "血缘"))
		[
			BloodlineWidget.ToSharedRef()
		];
}

void FUEAbilityKitAssetEditor::OnTaskSelected(UAKAbilityTaskBase* Task)
{
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(Task);
	}
}

void FUEAbilityKitAssetEditor::OnCursorTimeChanged(float Time)
{
	CursorTime = Time;
}

#undef LOCTEXT_NAMESPACE
