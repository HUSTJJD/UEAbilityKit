#include "UEAbilityKitAssetEditor.h"

#include "SUAKBloodlineView.h"
#include "SUAKGASImpactBar.h"
#include "SUAKPreviewViewport.h"
#include "SUAKTaskPicker.h"
#include "SUAKTimeline.h"
#include "SUAKValidationBar.h"
#include "UAKAbilityAsset.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"
#include "UAKTrace.h"
#include "UAKTypes.h"

#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FUEAbilityKitAssetEditor"

const FName FUEAbilityKitAssetEditor::TimelineTabId(TEXT("UAKTimeline"));
const FName FUEAbilityKitAssetEditor::DetailsTabId(TEXT("UAKDetails"));
const FName FUEAbilityKitAssetEditor::BloodlineTabId(TEXT("UAKBloodline"));
const FName FUEAbilityKitAssetEditor::PreviewTabId(TEXT("UAKPreview"));

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
					->AddTab(PreviewTabId, ETabState::ClosedTab)
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

	// PIE 联动：每 0.1s 检查一次正在运行的技能，让光标与血缘视图跟随
	PIETickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FUEAbilityKitAssetEditor::TickPIELink), 0.1f);
}

FUEAbilityKitAssetEditor::~FUEAbilityKitAssetEditor()
{
	if (PIETickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PIETickerHandle);
		PIETickerHandle.Reset();
	}
}

bool FUEAbilityKitAssetEditor::TickPIELink(float DeltaTime)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		return true;
	}

	// 找到 PIE 世界
	UWorld* PIEWorld = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.World())
		{
			PIEWorld = Context.World();
			break;
		}
	}

	if (!PIEWorld)
	{
		// PIE 已结束：血缘视图切回设计期
		if (LastLinkedTrace.IsValid() || bPIERecordingForced)
		{
			LastLinkedTrace.Reset();
			bPIERecordingForced = false;
			if (BloodlineWidget.IsValid())
			{
				BloodlineWidget->SetRuntimeTrace(nullptr);
			}
		}
		return true;
	}

	UAKTraceSubsystem* TraceSubsystem = PIEWorld->GetSubsystem<UAKTraceSubsystem>();
	if (!TraceSubsystem)
	{
		return true;
	}

	// PIE 期间自动开启记录，省去手动开关
	if (!TraceSubsystem->bRecordingEnabled)
	{
		TraceSubsystem->bRecordingEnabled = true;
		bPIERecordingForced = true;
	}

	// 找与本资产同名的最新一条溯源记录
	const UAKTrace* Match = nullptr;
	auto Scan = [&](const TArray<UAKTrace*>& Traces)
	{
		for (const UAKTrace* Trace : Traces)
		{
			if (!Trace || Trace->GetNodes().Num() == 0)
			{
				continue;
			}
			if (Trace->GetNodes()[0].Label == AssetPtr->GetName())
			{
				Match = Trace;
			}
		}
	};
	Scan(TraceSubsystem->GetActiveTraces());
	if (!Match)
	{
		Scan(TraceSubsystem->GetFinishedTraces());
	}

	if (!Match)
	{
		return true;
	}

	if (LastLinkedTrace.Get() != Match && BloodlineWidget.IsValid())
	{
		LastLinkedTrace = Match;
		BloodlineWidget->SetRuntimeTrace(Match);
	}

	// 光标跟随：取当前记录中最晚的节点时间
	const TArray<FAKTraceNode>& Nodes = Match->GetNodes();
	float MaxTime = 0.f;
	for (const FAKTraceNode& Node : Nodes)
	{
		MaxTime = FMath::Max(MaxTime, Node.Time);
	}

	if (TimelineWidget.IsValid() && !FMath::IsNearlyEqual(MaxTime, CursorTime, 0.001f))
	{
		TimelineWidget->SetCursorTime(MaxTime);
	}

	return true;
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

	InTabManager->RegisterTabSpawner(PreviewTabId,
		FOnSpawnTab::CreateRaw(this, &FUEAbilityKitAssetEditor::SpawnTab_Preview))
		.SetDisplayName(LOCTEXT("PreviewTab", "预览"))
		.SetGroup(WorkspaceMenu::GetMenuStructure()->GetAssetEditorCategory());
}

void FUEAbilityKitAssetEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(TimelineTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(BloodlineTabId);
	InTabManager->UnregisterTabSpawner(PreviewTabId);
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
		.OnTrackSelected(this, &FUEAbilityKitAssetEditor::OnTrackSelected)
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

	TaskPickerWidget = SNew(SUAKTaskPicker)
		.Asset(Asset)
		.OnAddTask(this, &FUEAbilityKitAssetEditor::OnAddTask);

	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTabLabel", "Inspector"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(280.f)
			[
				TaskPickerWidget.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				DetailsView.ToSharedRef()
			]
		];
}

TSharedRef<SDockTab> FUEAbilityKitAssetEditor::SpawnTab_Preview(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PreviewTabId);

	PreviewWidget = SNew(SUAKEditorViewport).Asset(Asset);

	return SNew(SDockTab)
		.Label(LOCTEXT("PreviewTabLabel", "预览"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("PreviewPlay", "播放"))
					.OnClicked(this, &FUEAbilityKitAssetEditor::OnPreviewPlayClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("PreviewStop", "停止"))
					.OnClicked(this, &FUEAbilityKitAssetEditor::OnPreviewStopClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("PreviewLoop", "循环"))
					.OnClicked(this, &FUEAbilityKitAssetEditor::OnPreviewLoopClicked)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				PreviewWidget.ToSharedRef()
			]
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

void FUEAbilityKitAssetEditor::OnTrackSelected(UAKAbilityTrack* Track)
{
	// 选中轨道：在 Inspector 里改复合模式（Sequence / Parallel / Repeat）与延迟
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(Track);
	}
}

void FUEAbilityKitAssetEditor::OnCursorTimeChanged(float Time)
{
	CursorTime = Time;
}

void FUEAbilityKitAssetEditor::OnAddTask(TSubclassOf<UAKAbilityTaskBase> TaskClass, int32 TrackIndex)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !TaskClass)
	{
		return;
	}

	AssetPtr->Modify();

	UAKAbilityTrack* Track = AssetPtr->Tracks.IsValidIndex(TrackIndex) ? AssetPtr->Tracks[TrackIndex] : nullptr;
	if (!Track)
	{
		Track = NewObject<UAKAbilityTrack>(AssetPtr);
		Track->TrackName = FName(TEXT("New Track"));
		Track->TrackType = EAKTrackType::Custom;
		AssetPtr->Tracks.Add(Track);
	}

	Track->Modify();

	UAKAbilityTaskBase* Task = NewObject<UAKAbilityTaskBase>(Track, TaskClass);
	Task->StartTime = FMath::Clamp(CursorTime, 0.f, AssetPtr->Length);
	Track->Tasks.Add(Task);

	AssetPtr->MarkPackageDirty();

	// 立刻在 Inspector 里选中新任务，方便直接改参数
	OnTaskSelected(Task);

	if (TaskPickerWidget.IsValid())
	{
		TaskPickerWidget->Refresh();
	}
}

FReply FUEAbilityKitAssetEditor::OnPreviewPlayClicked()
{
	if (PreviewWidget.IsValid())
	{
		return PreviewWidget->OnPlayClicked();
	}
	return FReply::Handled();
}

FReply FUEAbilityKitAssetEditor::OnPreviewStopClicked()
{
	if (PreviewWidget.IsValid())
	{
		return PreviewWidget->OnStopClicked();
	}
	return FReply::Handled();
}

FReply FUEAbilityKitAssetEditor::OnPreviewLoopClicked()
{
	if (PreviewWidget.IsValid())
	{
		PreviewWidget->ToggleLooping();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
