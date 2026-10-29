#include "UEAbilityKitAssetEditor.h"

#include "SUAKBloodlineView.h"
#include "SUAKGASImpactBar.h"
#include "SUAKPreviewViewport.h"
#include "SUAKTaskPicker.h"
#include "SUAKTimeline.h"
#include "SUAKTimelineToolbar.h"
#include "SUAKTrackOutliner.h"
#include "SUAKValidationBar.h"
#include "UAKAbilityAsset.h"
#include "UAKAbilityJsonSerializer.h"
#include "UAKSettings.h"

#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailsView.h"
#include "Textures/SlateIcon.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTask_PlayMontage.h"
#include "UAKAbilityTrack.h"
#include "UAKTrace.h"
#include "UAKTypes.h"

#include "Animation/AnimMontage.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FUEAbilityKitAssetEditor"

const FName FUEAbilityKitAssetEditor::TimelineTabId(TEXT("UAKTimeline"));
const FName FUEAbilityKitAssetEditor::DetailsTabId(TEXT("UAKDetails"));
const FName FUEAbilityKitAssetEditor::BloodlineTabId(TEXT("UAKBloodline"));
const FName FUEAbilityKitAssetEditor::PreviewTabId(TEXT("UAKPreview"));
const FName FUEAbilityKitAssetEditor::ValidationTabId(TEXT("UAKValidation"));

TArray<TWeakPtr<FUEAbilityKitAssetEditor>> FUEAbilityKitAssetEditor::OpenEditors;

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

	// 属性面板改完：预览角色 / 预览 Montage 变了要立刻重建预览。
	// 不挂这个回调的话，改完得重开编辑器才看得到新角色。
	// 这个版本没有 Set...()，是暴露委托成员后 AddLambda
	DetailsView->OnFinishedChangingProperties().AddLambda(
		[this](const FPropertyChangedEvent& InEvent)
		{
			static const FName PreviewCharacterClass(TEXT("PreviewCharacterClass"));
			static const FName PreviewMontage(TEXT("PreviewMontage"));

			const FName PropertyName = InEvent.GetPropertyName();
			if ((PropertyName == PreviewCharacterClass || PropertyName == PreviewMontage) && PreviewWidget.IsValid())
			{
				PreviewWidget->OnAssetChanged(Asset.Get());
			}
		});

	// 布局对齐 Sequencer 的心智：
	//   中间 —— 上预览、下时间轴（预览要跟着时间轴看，所以同列上下叠）
	//   右侧 —— Inspector 通高（改参数不必来回切标签）
	//   底部 —— 验证 / GAS 影响常驻（问题始终看得见；血缘同 Stack，切过去看因果）
	//
	// 布局名带版本号：TabManager 会把用户拖过的布局持久化，改名才能让这份新布局生效
	// （想恢复默认：Window → Reset Layout）。
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("UAKAbilityEditor_Layout_v4")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.82f)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)->SetSizeCoefficient(0.78f)
					->Split
					(
						FTabManager::NewStack()->SetSizeCoefficient(0.56f)
						->AddTab(PreviewTabId, ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewStack()->SetSizeCoefficient(0.44f)
						->AddTab(TimelineTabId, ETabState::OpenedTab)
					)
				)
				->Split
				(
					FTabManager::NewStack()->SetSizeCoefficient(0.22f)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
				)
			)
			->Split
			(
				FTabManager::NewStack()->SetSizeCoefficient(0.18f)
				->AddTab(ValidationTabId, ETabState::OpenedTab)
				->AddTab(BloodlineTabId, ETabState::OpenedTab)
			)
		);

	InitAssetEditor(Mode, InitToolkitHost, FName(TEXT("UAKAbilityEditorApp")), Layout,
		/*bCreateDefaultStandaloneMenu=*/true, /*bCreateDefaultToolbar=*/true, InAsset);

	// 顶部工具条（Save 那一行右侧本来空着一大片）：把预览控制放到那儿，
	// 预览面板里就不再单占一条控制条了。
	TSharedPtr<FExtender> ToolbarExtender = MakeShared<FExtender>();
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateRaw(this, &FUEAbilityKitAssetEditor::FillToolbar));
	AddToolbarExtender(ToolbarExtender);

	// 打开就先显示资产本身：长度 / 播放速率 / 循环 / 标记 / 标签都在这一层。
	// 以前默认是一片空白，得先在时间轴里点一下"选中资产"才看得到。
	SelectAssetInInspector();

	// PIE 联动：每 0.1s 检查一次正在运行的技能，让光标与血缘视图跟随
	PIETickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FUEAbilityKitAssetEditor::TickPIELink), 0.1f);

	// 撤销 / 重做：注册为 UndoClient，Ctrl+Z / Ctrl+Y 才能作用于本编辑器
	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}

	// 教程自动化：登记实例，UAK.Tutorial.* 靠它找到要驱动的那台编辑器
	OpenEditors.Add(TWeakPtr<FUEAbilityKitAssetEditor>(
		StaticCastSharedRef<FUEAbilityKitAssetEditor>(AsShared())));
}

FUEAbilityKitAssetEditor::~FUEAbilityKitAssetEditor()
{
	OpenEditors.RemoveAll([this](const TWeakPtr<FUEAbilityKitAssetEditor>& Weak)
	{
		const TSharedPtr<FUEAbilityKitAssetEditor> Pinned = Weak.Pin();
		return !Pinned.IsValid() || Pinned.Get() == this;
	});

	if (PIETickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PIETickerHandle);
		PIETickerHandle.Reset();
	}

	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
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
	auto Scan = [&](const TArray<TObjectPtr<UAKTrace>>& Traces)
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
	// UE 5.8 的 IWorkspaceMenuStructure 不再提供 GetAssetEditorCategory / GetStructureBuilder，
	// 这里只注册 Tab，不再往工作区菜单里挂分组（分组是可选的能力，丢了不影响功能）。
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(TimelineTabId,
		FOnSpawnTab::CreateRaw(this, &FUEAbilityKitAssetEditor::SpawnTab_Timeline))
		.SetDisplayName(LOCTEXT("TimelineTab", "时间轴"));

	InTabManager->RegisterTabSpawner(DetailsTabId,
		FOnSpawnTab::CreateRaw(this, &FUEAbilityKitAssetEditor::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTab", "Inspector"));

	InTabManager->RegisterTabSpawner(BloodlineTabId,
		FOnSpawnTab::CreateRaw(this, &FUEAbilityKitAssetEditor::SpawnTab_Bloodline))
		.SetDisplayName(LOCTEXT("BloodlineTab", "血缘"));

	InTabManager->RegisterTabSpawner(PreviewTabId,
		FOnSpawnTab::CreateRaw(this, &FUEAbilityKitAssetEditor::SpawnTab_Preview))
		.SetDisplayName(LOCTEXT("PreviewTab", "预览"));

	InTabManager->RegisterTabSpawner(ValidationTabId,
		FOnSpawnTab::CreateRaw(this, &FUEAbilityKitAssetEditor::SpawnTab_Validation))
		.SetDisplayName(LOCTEXT("ValidationTab", "验证 / GAS 影响"));
}

void FUEAbilityKitAssetEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(TimelineTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(BloodlineTabId);
	InTabManager->UnregisterTabSpawner(PreviewTabId);
	InTabManager->UnregisterTabSpawner(ValidationTabId);
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
		.OnCursorTimeChanged(this, &FUEAbilityKitAssetEditor::OnCursorTimeChanged)
		.OnRequestDeleteTask(FSimpleDelegate::CreateRaw(this, &FUEAbilityKitAssetEditor::DeleteSelectedTask))
		.OnRequestDeleteTasks(FOnAKDeleteTasks::CreateRaw(this, &FUEAbilityKitAssetEditor::DeleteTasks))
		.OnRequestDuplicateTask(FOnAKDuplicateTask::CreateRaw(this, &FUEAbilityKitAssetEditor::DuplicateSelectedTask))
		.OnRequestPasteTask(FOnAKPasteTask::CreateRaw(this, &FUEAbilityKitAssetEditor::PasteTaskFromJson))
		// 裸滚轮 / 跳行：垂直滚动归左栏管
		.OnRequestScrollBy(FOnAKTimelineScrollBy::CreateRaw(this, &FUEAbilityKitAssetEditor::ScrollTrackListBy))
		.OnRequestRevealRow(FOnAKTimelineRevealRow::CreateRaw(this, &FUEAbilityKitAssetEditor::RevealTrackRow))
		.OnAction(FOnAKToolbarAction::CreateRaw(this, &FUEAbilityKitAssetEditor::OnToolbarAction));
	TimelineWidget = Timeline;

	TSharedRef<SUAKTrackOutliner> Outliner = SNew(SUAKTrackOutliner)
		.Asset(Asset)
		.OnTrackSelected(this, &FUEAbilityKitAssetEditor::OnTrackSelected)
		.OnTracksChanged(FSimpleDelegate::CreateRaw(this, &FUEAbilityKitAssetEditor::RefreshAfterTrackChange))
		// 左栏滚多少，右侧就减多少：两边永远一行对一行
		.OnScrollOffsetChanged(FOnAKOutlinerScrollChanged::CreateLambda(
			[Timeline](float Offset) { Timeline->SetScrollOffset(Offset); }));
	TrackOutlinerWidget = Outliner;

	TSharedRef<SUAKTimelineToolbar> TimelineToolbar = SNew(SUAKTimelineToolbar)
		.Timeline(Timeline)
		.OnAction(FOnAKToolbarAction::CreateRaw(this, &FUEAbilityKitAssetEditor::OnToolbarAction));

	// 引导高亮锚点：官方教程按 SWidget::GetTag() 找控件
	ApplyTutorialTags(TimelineWidget, EUAKTutorialRegion::Timeline);
	ApplyTutorialTags(TrackOutlinerWidget, EUAKTutorialRegion::Timeline);

	OutlinerWidth = GetDefault<UUAKSettings>()->TimelineOutlinerWidth;

	return SNew(SDockTab)
		.Label(LOCTEXT("TimelineTabLabel", "时间轴"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.f)
			[
				TimelineToolbar
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				// 左右分栏抄 SSequencer.cpp:781-821：宽度可拖，拖完写回设置
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)
				.OnSplitterFinishedResizing_Lambda([this]()
				{
					if (UUAKSettings* Settings = GetMutableDefault<UUAKSettings>())
					{
						Settings->TimelineOutlinerWidth = OutlinerWidth;
						Settings->SaveConfig();
					}
				})
				+ SSplitter::Slot()
				.Resizable(true)
				.SizeRule(SSplitter::SizeToContent)
				.OnSlotResized_Lambda([this](const float InSlotSize)
				{
					OutlinerWidth = FMath::Max(SUAKTrackOutliner::MinWidth, InSlotSize);
				})
				[
					SNew(SBox)
					.MinDesiredWidth(SUAKTrackOutliner::MinWidth)
					.WidthOverride(this, &FUEAbilityKitAssetEditor::GetOutlinerWidth)
					[
						Outliner
					]
				]
				+ SSplitter::Slot()
				[
					Timeline
				]
			]
		];
}

TSharedRef<SDockTab> FUEAbilityKitAssetEditor::SpawnTab_Validation(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == ValidationTabId);

	// 校验与 GAS 影响移到独立的底部面板（Sequencer 里"问题始终看得见"的位置）。
	// 以前塞在时间轴 Tab 里，一旦切到别的标签就看不到了。
	TSharedRef<SUAKGASImpactBar> ImpactBar = SNew(SUAKGASImpactBar)
		.Asset(Asset)
		.CursorTime(this, &FUEAbilityKitAssetEditor::GetCursorTime)
		.OnEntryClicked(this, &FUEAbilityKitAssetEditor::RevealTaskInBloodline);
	ImpactBarWidget = ImpactBar;

	// 双击一条校验结果 → 选中并滚到出问题的那个片段
	ValidationBarWidget = SNew(SUAKValidationBar)
		.Asset(Asset)
		.OnIssueDoubleClicked(this, &FUEAbilityKitAssetEditor::RevealTaskInTimeline);

	ApplyTutorialTags(ImpactBarWidget, EUAKTutorialRegion::Validation);
	ApplyTutorialTags(ValidationBarWidget, EUAKTutorialRegion::Validation);

	return SNew(SDockTab)
		.Label(LOCTEXT("ValidationTabLabel", "验证 / GAS 影响"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder).Padding(2.f)
				[
					ImpactBar
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SBorder).Padding(2.f)
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

	ApplyTutorialTags(TaskPickerWidget, EUAKTutorialRegion::Details);
	if (DetailsView.IsValid())
	{
		ApplyTutorialTags(DetailsView, EUAKTutorialRegion::Details);
	}

	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTabLabel", "Inspector"))
		[
			SNew(SVerticalBox)
			// 面包屑：说清"现在在改谁"，并给一条随时回到资产属性的路
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("InspectAssetBtn", "技能资产"))
					.ToolTipText(LOCTEXT("InspectAssetBtnTip",
						"编辑技能本身：长度 / 播放速率 / 循环 / 标记 / 标签 / 中断顺序 / 预览角色"))
					.OnClicked_Lambda([this]()
					{
						SelectAssetInInspector();
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(FMargin(6.f, 0.f, 0.f, 0.f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &FUEAbilityKitAssetEditor::GetInspectedObjectLabel)
				]
			]
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

	// 只留视口：播放 / 停止 / 循环 / 角色都在顶部工具条，面板里不再重复占一条
	return SNew(SDockTab)
		.Label(LOCTEXT("PreviewTabLabel", "预览"))
		[
			PreviewWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FUEAbilityKitAssetEditor::SpawnTab_Bloodline(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == BloodlineTabId);

	// 双击血缘里的任意一行（含 Tag / GE / Cue 子行）→ 回跳时间轴上产生它的任务
	BloodlineWidget = SNew(SUAKBloodlineView)
		.Asset(Asset)
		.OnTaskFocusRequested(this, &FUEAbilityKitAssetEditor::RevealTaskInTimeline);
	ApplyTutorialTags(BloodlineWidget, EUAKTutorialRegion::Bloodline);

	return SNew(SDockTab)
		.Label(LOCTEXT("BloodlineTabLabel", "血缘"))
		[
			BloodlineWidget.ToSharedRef()
		];
}

void FUEAbilityKitAssetEditor::OnTaskSelected(UAKAbilityTaskBase* Task)
{
	SelectedTask = Task;
	if (DetailsView.IsValid())
	{
		// 没选中任务（点了空白处）就回到资产属性，而不是留一片空白的面板
		DetailsView->SetObject(Task ? static_cast<UObject*>(Task) : static_cast<UObject*>(Asset.Get()));
	}
}

void FUEAbilityKitAssetEditor::OnTrackSelected(UAKAbilityTrack* Track, int32 TrackIndex)
{
	// 选中轨道：在 Inspector 里改复合模式（Sequence / Parallel / Repeat）与延迟
	SelectedTrackIndex = TrackIndex;
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(Track ? static_cast<UObject*>(Track) : static_cast<UObject*>(Asset.Get()));
	}
	if (TimelineWidget.IsValid())
	{
		// 时间轴也得知道"当前哪条轨道"：粘贴任务要靠它决定落点
		TimelineWidget->SetSelectedTrackIndex(TrackIndex);
	}
	if (TrackOutlinerWidget.IsValid())
	{
		TrackOutlinerWidget->SetSelectedTrackIndex(TrackIndex);
	}
}

void FUEAbilityKitAssetEditor::ScrollTrackListBy(float DeltaPixels)
{
	if (TrackOutlinerWidget.IsValid())
	{
		TrackOutlinerWidget->ScrollBy(DeltaPixels);
	}
}

void FUEAbilityKitAssetEditor::RevealTrackRow(int32 Row)
{
	if (TrackOutlinerWidget.IsValid())
	{
		TrackOutlinerWidget->ScrollToRow(Row);
	}
}

void FUEAbilityKitAssetEditor::RefreshAfterTrackChange()
{
	if (TimelineWidget.IsValid())
	{
		TimelineWidget->ForceRefresh();
	}
	if (TaskPickerWidget.IsValid())
	{
		TaskPickerWidget->Refresh();
	}
	if (BloodlineWidget.IsValid())
	{
		BloodlineWidget->Invalidate(EInvalidateWidgetReason::Layout);
	}
}

FText FUEAbilityKitAssetEditor::GetInspectedObjectLabel() const
{
	if (const UAKAbilityTaskBase* Task = SelectedTask.Get())
	{
		return FText::Format(LOCTEXT("InspectedTask", "任务：{0}"), FText::FromString(Task->GetName()));
	}

	return LOCTEXT("InspectedAsset", "技能资产");
}

void FUEAbilityKitAssetEditor::DeleteSelectedTask()
{
	UAKAbilityTaskBase* Task = SelectedTask.Get();
	if (!Task)
	{
		return;
	}

	UAKAbilityTrack* Track = Cast<UAKAbilityTrack>(Task->GetOuter());
	if (!Track)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteTask", "删除任务"));
	Track->Modify();
	Track->Tasks.Remove(Task);

	if (UAKAbilityAsset* AssetPtr = Asset.Get())
	{
		AssetPtr->MarkPackageDirty();
	}

	SelectedTask = nullptr;
	if (DetailsView.IsValid())
	{
		// 回到资产属性，别把面板留空
		DetailsView->SetObject(Asset.Get());
	}
	if (TimelineWidget.IsValid())
	{
		TimelineWidget->ClearSelection();
	}
}

void FUEAbilityKitAssetEditor::DeleteTasks(const TArray<UAKAbilityTaskBase*>& Tasks)
{
	if (Tasks.Num() == 0)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteTasks", "删除任务"));

	for (UAKAbilityTaskBase* Task : Tasks)
	{
		UAKAbilityTrack* Track = Cast<UAKAbilityTrack>(Task->GetOuter());
		if (!Track)
		{
			continue;
		}

		Track->Modify();
		Track->Tasks.Remove(Task);
	}

	if (UAKAbilityAsset* AssetPtr = Asset.Get())
	{
		AssetPtr->MarkPackageDirty();
	}

	SelectedTask = nullptr;
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(Asset.Get());
	}
	if (TimelineWidget.IsValid())
	{
		// 选中集合里别留着已经删掉的任务
		TimelineWidget->ClearSelection();
		TimelineWidget->ForceRefresh();
	}
	if (TaskPickerWidget.IsValid())
	{
		TaskPickerWidget->Refresh();
	}
}

UAKAbilityTaskBase* FUEAbilityKitAssetEditor::PasteTaskFromJson(const FString& TaskJson)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || TaskJson.IsEmpty())
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Parsed;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TaskJson);
	if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
	{
		return nullptr;
	}

	FString TypeName;
	if (!Parsed->TryGetStringField(TEXT("$type"), TypeName))
	{
		return nullptr;
	}

	// 落点：优先当前选中轨道，其次选中任务所在轨道，最后新建一条
	UAKAbilityTrack* Track = AssetPtr->Tracks.IsValidIndex(SelectedTrackIndex)
		? AssetPtr->Tracks[SelectedTrackIndex] : nullptr;

	if (!Track && SelectedTask.IsValid())
	{
		Track = Cast<UAKAbilityTrack>(SelectedTask->GetOuter());
	}

	const FScopedTransaction Transaction(LOCTEXT("PasteTask", "粘贴任务"));

	if (!Track)
	{
		AssetPtr->Modify();
		Track = NewObject<UAKAbilityTrack>(AssetPtr);
		Track->TrackName = FName(*FString::Printf(TEXT("Track_%d"), AssetPtr->Tracks.Num()));
		AssetPtr->Tracks.Add(Track);
		SelectedTrackIndex = AssetPtr->Tracks.Num() - 1;
	}

	Track->Modify();
	UAKAbilityTaskBase* Pasted = UAKAbilityJsonSerializer::CreateTaskByTypeName(Track, TypeName);
	if (!Pasted)
	{
		return nullptr;
	}

	Pasted->ImportJson(*Parsed);

	// 从光标处开始，并留一份可区分的 TaskId（同一任务粘多次也认得出）
	Pasted->StartTime = FMath::Max(0.f, CursorTime);
	if (!Pasted->TaskId.IsNone())
	{
		Pasted->TaskId = FName(*FString::Printf(TEXT("%s_Copy"), *Pasted->TaskId.ToString()));
	}

	Track->Tasks.Add(Pasted);
	AssetPtr->MarkPackageDirty();

	if (TaskPickerWidget.IsValid())
	{
		TaskPickerWidget->Refresh();
	}

	OnTaskSelected(Pasted);
	return Pasted;
}

UAKAbilityTaskBase* FUEAbilityKitAssetEditor::DuplicateSelectedTask(bool bMergeIntoActiveTransaction)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	UAKAbilityTaskBase* Task = SelectedTask.Get();
	if (!AssetPtr || !Task)
	{
		return nullptr;
	}

	UAKAbilityTrack* Track = Cast<UAKAbilityTrack>(Task->GetOuter());
	if (!Track)
	{
		return nullptr;
	}

	// Alt+拖拽时时间轴已经开好事务，这里不再开一个，否则一次操作要撤销两下
	TUniquePtr<FScopedTransaction> Transaction;
	if (!bMergeIntoActiveTransaction)
	{
		Transaction = MakeUnique<FScopedTransaction>(LOCTEXT("DuplicateTask", "复制任务"));
	}
	Track->Modify();

	UAKAbilityTaskBase* Copy = DuplicateObject<UAKAbilityTaskBase>(Task, Track);
	if (!Copy)
	{
		return nullptr;
	}

	// 复制出来的任务紧接在原任务之后
	Copy->StartTime = FMath::Min(Task->GetEndTime(), AssetPtr->Length);
	Track->Tasks.Add(Copy);
	AssetPtr->MarkPackageDirty();

	OnTaskSelected(Copy);
	return Copy;
}

void FUEAbilityKitAssetEditor::OnToolbarAction(EAKToolbarAction Action)
{
	switch (Action)
	{
	// 轨道增删/排序统一走左栏：只有那里才知道"新轨道该插在哪、哪条被锁了"
	case EAKToolbarAction::AddTrack:
		if (TrackOutlinerWidget.IsValid()) { TrackOutlinerWidget->AddTrack(); } else { AddTrack(); }
		break;
	case EAKToolbarAction::RemoveTrack:
		if (TrackOutlinerWidget.IsValid()) { TrackOutlinerWidget->RemoveTrack(SelectedTrackIndex); } else { RemoveSelectedTrack(); }
		break;
	case EAKToolbarAction::MoveTrackUp:
		if (TrackOutlinerWidget.IsValid()) { TrackOutlinerWidget->MoveTrack(SelectedTrackIndex, -1); } else { MoveSelectedTrack(-1); }
		break;
	case EAKToolbarAction::MoveTrackDown:
		if (TrackOutlinerWidget.IsValid()) { TrackOutlinerWidget->MoveTrack(SelectedTrackIndex, 1); } else { MoveSelectedTrack(1); }
		break;
	case EAKToolbarAction::DuplicateTask:		DuplicateSelectedTask(); break;
	case EAKToolbarAction::DeleteTask:			DeleteSelectedTask(); break;
	case EAKToolbarAction::AddMarker:			AddMarkerAtCursor(); break;
	case EAKToolbarAction::RemoveMarker:
		RemoveMarkerAt(TimelineWidget.IsValid() ? TimelineWidget->GetContextMarkerIndex() : INDEX_NONE);
		break;
	case EAKToolbarAction::SelectAsset:			SelectAssetInInspector(); break;
	case EAKToolbarAction::GenerateFromMontage:	GenerateFromMontage(); break;
	default: break;
	}
}

void FUEAbilityKitAssetEditor::AddTrack()
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddTrack", "添加轨道"));
	AssetPtr->Modify();

	UAKAbilityTrack* Track = NewObject<UAKAbilityTrack>(AssetPtr);
	Track->TrackName = FName(*FString::Printf(TEXT("Track_%d"), AssetPtr->Tracks.Num()));
	Track->TrackType = EAKTrackType::Custom;
	Track->CompositeMode = EAKTrackCompositeMode::Timeline;
	AssetPtr->Tracks.Add(Track);

	AssetPtr->MarkPackageDirty();
	SelectedTrackIndex = AssetPtr->Tracks.Num() - 1;

	if (TaskPickerWidget.IsValid())
	{
		TaskPickerWidget->Refresh();
	}
}

void FUEAbilityKitAssetEditor::RemoveSelectedTrack()
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(SelectedTrackIndex))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("RemoveTrack", "删除轨道"));
	AssetPtr->Modify();
	AssetPtr->Tracks.RemoveAt(SelectedTrackIndex);
	AssetPtr->MarkPackageDirty();

	SelectedTrackIndex = INDEX_NONE;
	SelectedTask = nullptr;

	if (DetailsView.IsValid())
	{
		// 回到资产属性，别把面板留空
		DetailsView->SetObject(Asset.Get());
	}
	if (TaskPickerWidget.IsValid())
	{
		TaskPickerWidget->Refresh();
	}
}

void FUEAbilityKitAssetEditor::MoveSelectedTrack(int32 Delta)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Tracks.IsValidIndex(SelectedTrackIndex))
	{
		return;
	}

	const int32 Target = SelectedTrackIndex + Delta;
	if (!AssetPtr->Tracks.IsValidIndex(Target))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("MoveTrack", "移动轨道"));
	AssetPtr->Modify();
	AssetPtr->Tracks.Swap(SelectedTrackIndex, Target);
	AssetPtr->MarkPackageDirty();
	SelectedTrackIndex = Target;
}

void FUEAbilityKitAssetEditor::AddMarkerAtCursor()
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddMarker", "添加标记"));
	AssetPtr->Modify();

	FAKTimeMarker Marker;
	Marker.Name = FName(*FString::Printf(TEXT("Marker_%d"), AssetPtr->Markers.Num()));
	Marker.Time = CursorTime;
	AssetPtr->Markers.Add(Marker);

	AssetPtr->MarkPackageDirty();
}

void FUEAbilityKitAssetEditor::RemoveMarkerAt(int32 MarkerIndex)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !AssetPtr->Markers.IsValidIndex(MarkerIndex))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("RemoveMarker", "删除标记"));
	AssetPtr->Modify();
	AssetPtr->Markers.RemoveAt(MarkerIndex);
	AssetPtr->MarkPackageDirty();

	if (TimelineWidget.IsValid())
	{
		TimelineWidget->ClearContextMarkerIndex();
		TimelineWidget->ForceRefresh();
	}
}

void FUEAbilityKitAssetEditor::SelectAssetInInspector()
{
	// 让长度、Tags、Markers、中断顺序等资产级属性可在 Inspector 里编辑
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(Asset.Get());
	}
}

void FUEAbilityKitAssetEditor::GenerateFromMontage()
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		return;
	}

	UAnimMontage* Montage = AssetPtr->PreviewMontage.LoadSynchronous();
	if (!Montage)
	{
		// 没有预览 Montage：先把资产选中，方便用户去填
		SelectAssetInInspector();
		return;
	}

	const float MontageLength = Montage->GetPlayLength();
	if (MontageLength <= 0.f)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("GenerateFromMontage", "从 Montage 生成"));
	AssetPtr->Modify();

	// 1) 按 Montage 长度设置技能长度
	AssetPtr->Length = MontageLength;

	// 2) 找到或新建 Main 轨道
	UAKAbilityTrack* MainTrack = nullptr;
	for (UAKAbilityTrack* Track : AssetPtr->Tracks)
	{
		if (Track && Track->TrackType == EAKTrackType::Main)
		{
			MainTrack = Track;
			break;
		}
	}

	if (!MainTrack)
	{
		MainTrack = NewObject<UAKAbilityTrack>(AssetPtr);
		MainTrack->TrackName = FName(TEXT("Main"));
		MainTrack->TrackType = EAKTrackType::Main;
		MainTrack->CompositeMode = EAKTrackCompositeMode::Timeline;
		AssetPtr->Tracks.Add(MainTrack);
	}

	MainTrack->Modify();

	// 3) 放一个覆盖全长的 PlayMontage 任务（已存在则跳过）
	bool bAlreadyHasMontageTask = false;
	for (const UAKAbilityTaskBase* Task : MainTrack->Tasks)
	{
		if (const UAKAbilityTask_PlayMontage* Existing = Cast<UAKAbilityTask_PlayMontage>(Task))
		{
			if (Existing->Montage == Montage)
			{
				bAlreadyHasMontageTask = true;
				break;
			}
		}
	}

	if (!bAlreadyHasMontageTask)
	{
		UAKAbilityTask_PlayMontage* PlayTask = NewObject<UAKAbilityTask_PlayMontage>(MainTrack);
		PlayTask->Montage = Montage;
		PlayTask->StartTime = 0.f;
		PlayTask->Duration = MontageLength;
		MainTrack->Tasks.Add(PlayTask);
	}

	// 4) 把动画 Notify 转成命名时间点（比"只按长度裁剪"多做一步）
	for (const FAnimNotifyEvent& Notify : Montage->Notifies)
	{
		FAKTimeMarker Marker;
		Marker.Name = Notify.NotifyName;
		Marker.Time = Notify.GetTriggerTime();

		// 同名同时间的标记不重复添加
		bool bExists = false;
		for (const FAKTimeMarker& Existing : AssetPtr->Markers)
		{
			if (Existing.Name == Marker.Name && FMath::IsNearlyEqual(Existing.Time, Marker.Time, 0.001f))
			{
				bExists = true;
				break;
			}
		}

		if (!bExists)
		{
			AssetPtr->Markers.Add(Marker);
		}
	}

	AssetPtr->MarkPackageDirty();

	if (TaskPickerWidget.IsValid())
	{
		TaskPickerWidget->Refresh();
	}
}

void FUEAbilityKitAssetEditor::OnCursorTimeChanged(float Time)
{
	CursorTime = Time;
}

void FUEAbilityKitAssetEditor::RevealTaskInTimeline(UAKAbilityTaskBase* Task)
{
	if (!Task)
	{
		// 资产级问题没有可跳的片段：把 Inspector 收回资产，别停在某个无关任务上
		SelectAssetInInspector();
		return;
	}

	OnTaskSelected(Task);

	if (TimelineWidget.IsValid())
	{
		TimelineWidget->FocusTask(Task);
	}

	AutomationFocusTab(TimelineTabId);
}

void FUEAbilityKitAssetEditor::RevealTaskInBloodline(UAKAbilityTaskBase* Task)
{
	RevealTaskInTimeline(Task);
	AutomationFocusTab(BloodlineTabId);
}

void FUEAbilityKitAssetEditor::OnAddTask(TSubclassOf<UAKAbilityTaskBase> TaskClass, int32 TrackIndex)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr || !TaskClass)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddTask", "添加任务"));
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

void FUEAbilityKitAssetEditor::PostUndo(bool bSuccess)
{
	RefreshAfterUndo();
}

void FUEAbilityKitAssetEditor::PostRedo(bool bSuccess)
{
	RefreshAfterUndo();
}

void FUEAbilityKitAssetEditor::RefreshAfterUndo()
{
	// 撤销后内容可能完全变了：强制重建执行计划、刷新选择器，并清掉失效的选中
	if (TimelineWidget.IsValid())
	{
		TimelineWidget->ForceRefresh();
	}

	if (TaskPickerWidget.IsValid())
	{
		TaskPickerWidget->Refresh();
	}

	if (!SelectedTask.IsValid())
	{
		SelectedTask = nullptr;
		if (DetailsView.IsValid())
		{
			// 没有选中项时显示资产本身
			DetailsView->SetObject(Asset.Get());
		}
	}
	else if (DetailsView.IsValid())
	{
		DetailsView->SetObject(SelectedTask.Get());
	}
}

FReply FUEAbilityKitAssetEditor::OnPreviewLoopClicked()
{
	if (PreviewWidget.IsValid())
	{
		PreviewWidget->ToggleLooping();
	}
	return FReply::Handled();
}

TSharedRef<SWidget> FUEAbilityKitAssetEditor::MakePreviewCharacterMenu()
{
	if (PreviewWidget.IsValid())
	{
		return PreviewWidget->MakePreviewCharacterMenu();
	}
	return SNullWidget::NullWidget;
}

void FUEAbilityKitAssetEditor::FillToolbar(FToolBarBuilder& Builder)
{
	Builder.BeginSection(TEXT("UAKPreview"));
	{
		Builder.AddToolBarButton(
			// OnPreviewPlayClicked 返回 FReply，FExecuteAction 是 void()，用 lambda 包一层
			FUIAction(FExecuteAction::CreateLambda([this]() { OnPreviewPlayClicked(); })),
			NAME_None,
			LOCTEXT("ToolbarPlay", "播放"),
			LOCTEXT("ToolbarPlayTip", "从 0 跑一次这个技能：Montage / GameplayCue / 位移都真的发生，跑到结束自动停"),
			FSlateIcon());

		Builder.AddToolBarButton(
			FUIAction(FExecuteAction::CreateLambda([this]() { OnPreviewStopClicked(); })),
			NAME_None,
			LOCTEXT("ToolbarStop", "停止"),
			LOCTEXT("ToolbarStopTip", "中断这次施放，并收掉残留的 Montage 与 Cue"),
			FSlateIcon());

		Builder.AddToolBarButton(
			FUIAction(FExecuteAction::CreateLambda([this]() { OnPreviewLoopClicked(); })),
			NAME_None,
			LOCTEXT("ToolbarLoop", "循环"),
			LOCTEXT("ToolbarLoopTip", "跑完自动重开一次（属性不清零，连放会累积）"),
			FSlateIcon());

		Builder.AddToolBarButton(
			FUIAction(FExecuteAction::CreateLambda([this]()
			{
				if (PreviewWidget.IsValid())
				{
					PreviewWidget->ResetCamera();
				}
			})),
			NAME_None,
			LOCTEXT("ToolbarResetCamera", "复位视角"),
			LOCTEXT("ToolbarResetCameraTip", "相机回到侧视：从角色侧面平视"),
			FSlateIcon());

		Builder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateRaw(this, &FUEAbilityKitAssetEditor::MakePreviewCharacterMenu),
			TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([this]()
			{
				return PreviewWidget.IsValid() ? PreviewWidget->GetPreviewCharacterLabel() : FText::GetEmpty();
			})),
			LOCTEXT("ToolbarCharTip", "这个技能用哪只角色预览；不指定就跟随工程默认"),
			FSlateIcon());
	}
	Builder.EndSection();
}

// ─────────────────────────────────────────────────────────────────────────
// 教程自动化桥接：只做转发，不含业务逻辑
// ─────────────────────────────────────────────────────────────────────────
TSharedPtr<FUEAbilityKitAssetEditor> FUEAbilityKitAssetEditor::FindEditorForAsset(const UAKAbilityAsset* InAsset)
{
	TSharedPtr<FUEAbilityKitAssetEditor> Fallback;

	for (int32 Index = OpenEditors.Num() - 1; Index >= 0; --Index)
	{
		TSharedPtr<FUEAbilityKitAssetEditor> Editor = OpenEditors[Index].Pin();
		if (!Editor.IsValid())
		{
			OpenEditors.RemoveAt(Index);
			continue;
		}

		if (!InAsset)
		{
			Fallback = Editor;
			break;
		}
		if (Editor->Asset.Get() == InAsset)
		{
			return Editor;
		}
	}

	return Fallback;
}

FName FUEAbilityKitAssetEditor::GetTabIdForName(const FString& Name)
{
	if (Name.Equals(TEXT("Bloodline"), ESearchCase::IgnoreCase)) { return BloodlineTabId; }
	if (Name.Equals(TEXT("Details"), ESearchCase::IgnoreCase)
		|| Name.Equals(TEXT("Inspector"), ESearchCase::IgnoreCase)) { return DetailsTabId; }
	if (Name.Equals(TEXT("Preview"), ESearchCase::IgnoreCase)) { return PreviewTabId; }
	if (Name.Equals(TEXT("Validation"), ESearchCase::IgnoreCase)) { return ValidationTabId; }
	return TimelineTabId;
}

TSharedPtr<SWidget> FUEAbilityKitAssetEditor::GetWidgetForRegion(EUAKTutorialRegion Region)
{
	auto TabContent = [this](const FName& TabId) -> TSharedPtr<SWidget>
	{
		if (GetTabManager().IsValid())
		{
			if (const TSharedPtr<SDockTab> Tab = GetTabManager()->FindExistingLiveTab(TabId))
			{
				return Tab->GetContent();
			}
		}
		return nullptr;
	};

	switch (Region)
	{
	case EUAKTutorialRegion::Timeline:
		return TimelineWidget.IsValid() ? TimelineWidget : TabContent(TimelineTabId);

	case EUAKTutorialRegion::Bloodline:
		return BloodlineWidget.IsValid() ? BloodlineWidget : TabContent(BloodlineTabId);

	case EUAKTutorialRegion::Details:
		return TabContent(DetailsTabId);

	case EUAKTutorialRegion::Validation:
		return ValidationBarWidget;

	case EUAKTutorialRegion::Editor:
		if (GetTabManager().IsValid())
		{
			if (const TSharedPtr<SDockTab> Owner = GetTabManager()->GetOwnerTab())
			{
				return Owner->GetContent();
			}
		}
		return TabContent(TimelineTabId);

	default:
		return TabContent(TimelineTabId);
	}
}

void FUEAbilityKitAssetEditor::AutomationFocusTab(FName TabId)
{
	if (!GetTabManager().IsValid())
	{
		return;
	}

	if (const TSharedPtr<SDockTab> Tab = GetTabManager()->TryInvokeTab(TabId))
	{
		Tab->ActivateInParent(ETabActivationCause::SetDirectly);
		Tab->DrawAttention();
	}
}

void FUEAbilityKitAssetEditor::AutomationRefresh()
{
	if (TimelineWidget.IsValid())
	{
		TimelineWidget->ForceRefresh();
	}
	if (BloodlineWidget.IsValid())
	{
		BloodlineWidget->Invalidate(EInvalidateWidgetReason::Layout);
	}
	if (ImpactBarWidget.IsValid())
	{
		ImpactBarWidget->Invalidate(EInvalidateWidgetReason::Layout);
	}
	if (ValidationBarWidget.IsValid())
	{
		ValidationBarWidget->Invalidate(EInvalidateWidgetReason::Layout);
	}
}

void FUEAbilityKitAssetEditor::AutomationSetCursorTime(float InTime)
{
	CursorTime = InTime;
	if (TimelineWidget.IsValid())
	{
		TimelineWidget->SetCursorTime(InTime);
	}
	OnCursorTimeChanged(InTime);
}

void FUEAbilityKitAssetEditor::AutomationPlay()
{
	if (TimelineWidget.IsValid())
	{
		TimelineWidget->Play();
	}
}

void FUEAbilityKitAssetEditor::AutomationPause()
{
	if (TimelineWidget.IsValid())
	{
		TimelineWidget->Pause();
	}
}

void FUEAbilityKitAssetEditor::AutomationStop()
{
	if (TimelineWidget.IsValid())
	{
		TimelineWidget->Stop();
	}
}

void FUEAbilityKitAssetEditor::AutomationStepFrames(int32 Frames)
{
	if (TimelineWidget.IsValid())
	{
		TimelineWidget->StepFrame(Frames);
		CursorTime = TimelineWidget->GetCursorTime();
	}
}

void FUEAbilityKitAssetEditor::AutomationSetShowFrames(bool bInShowFrames)
{
	if (TimelineWidget.IsValid())
	{
		TimelineWidget->SetShowFrames(bInShowFrames);
	}
}

void FUEAbilityKitAssetEditor::AutomationSelectTask(UAKAbilityTaskBase* Task)
{
	OnTaskSelected(Task);
}

void FUEAbilityKitAssetEditor::AutomationSelectTrack(int32 TrackIndex)
{
	SelectedTrackIndex = TrackIndex;
	if (Asset.IsValid() && Asset->Tracks.IsValidIndex(TrackIndex))
	{
		OnTrackSelected(Asset->Tracks[TrackIndex], TrackIndex);
	}
}

void FUEAbilityKitAssetEditor::ApplyTutorialTags(const TSharedPtr<SWidget>& Widget, EUAKTutorialRegion Region)
{
	if (Widget.IsValid())
	{
		Widget->SetTag(UAKTutorialTagForRegion(Region));
	}
}

FUEAbilityKitAssetEditor::FGuidedStats FUEAbilityKitAssetEditor::GetGuidedStats(bool bNeedValidation) const
{
	FGuidedStats Stats;
	Stats.CursorTime = GetCursorTime();
	Stats.bHasSelectedTask = SelectedTask.IsValid();
	Stats.bHasSelectedTrack = (SelectedTrackIndex != INDEX_NONE);

	if (Asset.IsValid())
	{
		Stats.Tracks = Asset->Tracks.Num();
		for (const UAKAbilityTrack* Track : Asset->Tracks)
		{
			if (Track)
			{
				Stats.Tasks += Track->Tasks.Num();
			}
		}

		if (bNeedValidation)
		{
			FAKValidationResult Result;
			Asset->Validate(Result);
			Stats.Errors = Result.NumErrors();
			Stats.Warnings = Result.NumWarnings();
		}
	}

	return Stats;
}

#undef LOCTEXT_NAMESPACE
