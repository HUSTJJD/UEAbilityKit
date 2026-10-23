#include "UAKTutorialGuided.h"

#include "UAKTutorialAutomation.h"
#include "UEAbilityKitAssetEditor.h"

#include "UAKAbilityAsset.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "HAL/ConsoleManager.h"
#include "Misc/PackageName.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"

#if UAK_WITH_INTRO_TUTORIALS
#include "EditorTutorial.h"
#include "IIntroTutorials.h"
#endif // UAK_WITH_INTRO_TUTORIALS

// ─────────────────────────────────────────────────────────────────────────
// 引导会话状态
//
// 只有一个会话（同一时刻引导一条教程就够了），所以放在文件作用域，
// 不搞单例里套单例。
// ─────────────────────────────────────────────────────────────────────────
namespace
{
	bool bGuidedRunning = false;
	FUAKTutorialScript GScript;
	int32 GCurrentStage = 0;
	float GStageElapsed = 0.f;
	FTSTicker::FDelegateHandle GTickerHandle;
	TArray<IConsoleObject*> GConsoleCommands;

#if UAK_WITH_INTRO_TUTORIALS
	TWeakObjectPtr<UEditorTutorial> GTutorial;
#endif

	/**
	 * 目标状态达成判定。
	 *
	 * 只比"状态"，不比"操作序列"——读者可以先删了重加、可以先加任务后加轨道，
	 * 只要最终状态对得上就放行。这是引导与录制的分界线：
	 * 录制要求严格按脚本走，引导只要求走到该到的地方。
	 */
	bool IsSatisfied(const FUAKTutorialAdvance& A, const FUEAbilityKitAssetEditor::FGuidedStats& S)
	{
		if (A.IsManual())
		{
			return false;   // 脚本没写条件：这一步由读者自己点「下一步」
		}
		if (A.MinTracks != INDEX_NONE && S.Tracks < A.MinTracks)                 { return false; }
		if (A.MinTasks != INDEX_NONE && S.Tasks < A.MinTasks)                    { return false; }
		if (A.MinCursorTime >= 0.f && S.CursorTime + KINDA_SMALL_NUMBER < A.MinCursorTime) { return false; }
		if (A.bRequireSelectedTask && !S.bHasSelectedTask)                       { return false; }
		if (A.bRequireSelectedTrack && !S.bHasSelectedTrack)                     { return false; }
		if (A.ExpectErrors != INDEX_NONE && S.Errors != A.ExpectErrors)          { return false; }
		return true;
	}

	bool AnyStageNeedsValidation()
	{
		for (const FUAKTutorialStep& Step : GScript.Steps)
		{
			if (Step.Advance.ExpectErrors != INDEX_NONE)
			{
				return true;
			}
		}
		return false;
	}

	/** 推进到下一个 stage（宏关闭时是空操作，引导整体不可用但不会崩） */
	void AdvanceStage()
	{
#if UAK_WITH_INTRO_TUTORIALS
		// GoToNextTutorialStage 是 protected 的静态函数，C++ 侧直接调不到符号；
		// 但它同时是 BlueprintCallable 的 UFUNCTION，反射表里挂着 exec 包装。
		// 跟蓝图一样，在 CDO 上 ProcessEvent 一次即可（静态函数的 exec 不读 this）。
		if (UClass* TutorialClass = UEditorTutorial::StaticClass())
		{
			if (UFunction* GoNext = TutorialClass->FindFunctionByName(TEXT("GoToNextTutorialStage")))
			{
				TutorialClass->GetDefaultObject()->ProcessEvent(GoNext, nullptr);
			}
		}
#endif
		++GCurrentStage;
		GStageElapsed = 0.f;
	}

	/** 打开（或新建）教程用的技能资产——与录制模式同一个起点 */
	UAKAbilityAsset* PrepareAsset(const FUAKTutorialScript& Script)
	{
		const FString AssetName = FPackageName::GetShortName(Script.Asset.Path);
		UPackage* Package = CreatePackage(*Script.Asset.Path);
		if (!Package)
		{
			return nullptr;
		}
		Package->FullyLoad();

		UAKAbilityAsset* Asset = FindObject<UAKAbilityAsset>(Package, *AssetName);
		if (!Asset)
		{
			Asset = NewObject<UAKAbilityAsset>(Package, UAKAbilityAsset::StaticClass(),
				*AssetName, RF_Public | RF_Standalone);
		}

		Asset->Length = Script.Asset.Length;
		Asset->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Asset);

		if (GEditor)
		{
			if (UAssetEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				Subsystem->OpenEditorForAsset(Asset);
			}
		}

		return Asset;
	}

	/** 一步的教学文本：讲解 + 操作 + 预期，顺序固定，读者扫一眼就知道三段各自是什么 */
	FText BuildStageText(const FUAKTutorialStep& Step, int32 Index, int32 Total)
	{
		FString Text = FString::Printf(TEXT("第 %d / %d 步　%s\n\n"), Index + 1, Total, *Step.Title);
		if (!Step.Say.IsEmpty())    { Text += FString::Printf(TEXT("为什么这么做：%s\n\n"), *Step.Say); }
		if (!Step.Do.IsEmpty())     { Text += FString::Printf(TEXT("操作：%s\n\n"), *Step.Do); }
		if (!Step.Expect.IsEmpty()) { Text += FString::Printf(TEXT("应该看到：%s"), *Step.Expect); }
		return FText::FromString(Text);
	}

	bool Tick(float DeltaTime)
	{
		if (!bGuidedRunning)
		{
			return false;
		}

		const TSharedPtr<FUEAbilityKitAssetEditor> Editor = FUEAbilityKitAssetEditor::FindEditorForAsset(nullptr);
		if (!Editor.IsValid() || GCurrentStage >= GScript.Steps.Num())
		{
			GStageElapsed += DeltaTime;
			return true;
		}

		const FUEAbilityKitAssetEditor::FGuidedStats Stats = Editor->GetGuidedStats(AnyStageNeedsValidation());
		const FUAKTutorialStep& Step = GScript.Steps[GCurrentStage];

		GStageElapsed += DeltaTime;
		const bool bTimedOut = Step.Advance.Timeout > 0.f && GStageElapsed >= Step.Advance.Timeout;

		// 只在"恰好下一步达成"时推进：读者手动跳步时我们不会连推过头
		if (bTimedOut || IsSatisfied(Step.Advance, Stats))
		{
			const FName TabId = FUEAbilityKitAssetEditor::GetTabIdForName(
				Step.Capture.Region == EUAKTutorialRegion::Bloodline ? TEXT("Bloodline")
				: Step.Capture.Region == EUAKTutorialRegion::Details ? TEXT("Details")
				: TEXT("Timeline"));
			Editor->AutomationFocusTab(TabId);
			AdvanceStage();
		}

		return true;
	}

	void StopInternal()
	{
		if (GTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(GTickerHandle);
			GTickerHandle.Reset();
		}
		bGuidedRunning = false;
		GCurrentStage = 0;
		GStageElapsed = 0.f;
	}

	void OnTutorialClosed()
	{
		StopInternal();
		UE_LOG(LogTemp, Log, TEXT("[UAK] 引导教程已结束：%s"), *GScript.Id);
	}

	void CmdGuided(const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UAK] 用法：UAK.Tutorial.Guided <教程id|脚本路径>"));
			return;
		}

		FString Error;
		if (!FUAKTutorialGuided::Get().Start(Args[0], Error))
		{
			UE_LOG(LogTemp, Error, TEXT("[UAK] %s"), *Error);
		}
	}

	void CmdGuidedStop(const TArray<FString>& Args)
	{
		FUAKTutorialGuided::Get().Stop();
	}
}

// ─────────────────────────────────────────────────────────────────────────
FUAKTutorialGuided& FUAKTutorialGuided::Get()
{
	static FUAKTutorialGuided Instance;
	return Instance;
}

bool FUAKTutorialGuided::IsAvailable() const
{
#if UAK_WITH_INTRO_TUTORIALS
	return IIntroTutorials::IsAvailable();
#else
	return false;
#endif
}

bool FUAKTutorialGuided::IsRunning() const
{
	return bGuidedRunning;
}

bool FUAKTutorialGuided::Start(const FString& TutorialOrIdOrPath, FString& OutError)
{
	if (!IsAvailable())
	{
		OutError = TEXT("引导模式不可用：引擎的 IntroTutorials（GuidedTutorials 插件）没启用。"
			"请在插件浏览器里启用它；或改用 UAK.Tutorial.Run / Tools/generate_tutorials.ps1 "
			"看自动录制生成的文档。");
		return false;
	}

	FString Path = TutorialOrIdOrPath;
	if (!Path.Contains(TEXT("/")) && !Path.Contains(TEXT("\\")))
	{
		Path = FUAKTutorialAutomation::GetScriptDir() / (Path + TEXT(".json"));
	}

	FUAKTutorialScript Script;
	if (!FUAKTutorialScript::LoadFromFile(Path, Script, OutError))
	{
		return false;
	}

	if (bGuidedRunning)
	{
		Stop();
	}

	GScript = Script;
	GCurrentStage = 0;
	GStageElapsed = 0.f;

	PrepareAsset(Script);

#if UAK_WITH_INTRO_TUTORIALS
	// 运行时构造，不落盘成 .uasset：教程内容始终以 JSON 为准，随时可重算
	UEditorTutorial* Tutorial = NewObject<UEditorTutorial>(GetTransientPackage());
	Tutorial->Title = FText::FromString(Script.Title);
	Tutorial->Category = TEXT("UEAbilityKit");
	Tutorial->SummaryContent.Type = ETutorialContent::Text;
	Tutorial->SummaryContent.Text = FText::FromString(Script.Summary);
	Tutorial->AssetToUse = FSoftObjectPath(Script.Asset.Path);

	for (int32 Index = 0; Index < Script.Steps.Num(); ++Index)
	{
		const FUAKTutorialStep& Step = Script.Steps[Index];

		FTutorialStage Stage;
		Stage.Name = FName(*Step.Id);
		Stage.Content.Type = ETutorialContent::Text;
		Stage.Content.Text = BuildStageText(Step, Index, Script.Steps.Num());
		Stage.NextButtonText = Step.Advance.IsManual()
			? FText::FromString(TEXT("我做好了，下一步"))
			: FText::FromString(TEXT("跳过这一步"));

		// 高亮锚点：官方按 SWidget::GetTag() 匹配，UAK 面板已经打好了 UAK.* 的 Tag
		FTutorialWidgetContent WidgetContent;
		WidgetContent.Content.Type = ETutorialContent::Text;
		WidgetContent.Content.Text = FText::FromString(Step.Do.IsEmpty() ? Step.Title : Step.Do);
		WidgetContent.WidgetAnchor.Type = ETutorialAnchorIdentifier::NamedWidget;
		WidgetContent.WidgetAnchor.WrapperIdentifier = UAKTutorialTagForRegion(Step.Capture.Region);
		WidgetContent.WidgetAnchor.bDrawHighlight = true;
		Stage.WidgetContent.Add(WidgetContent);

		Tutorial->Stages.Add(Stage);
	}

	GTutorial = Tutorial;
	IIntroTutorials::Get().LaunchTutorial(
		Tutorial,
		IIntroTutorials::TST_RESTART,
		nullptr,
		FSimpleDelegate::CreateStatic(&OnTutorialClosed),
		FSimpleDelegate::CreateStatic(&OnTutorialClosed));
#endif // UAK_WITH_INTRO_TUTORIALS

	bGuidedRunning = true;
	GTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.25f);

	UE_LOG(LogTemp, Log, TEXT("[UAK] 引导模式开始：%s（%d 步）。做对了会自动进下一步。"),
		*Script.Id, Script.Steps.Num());
	return true;
}

void FUAKTutorialGuided::Stop()
{
	if (!bGuidedRunning)
	{
		return;
	}

#if UAK_WITH_INTRO_TUTORIALS
	if (IIntroTutorials::IsAvailable())
	{
		IIntroTutorials::Get().CloseAllTutorialContent();
	}
#endif

	StopInternal();
}

void FUAKTutorialGuided::Register()
{
#if UAK_WITH_INTRO_TUTORIALS
	if (IIntroTutorials::IsAvailable())
	{
		FTutorialCategory Category;
		Category.Identifier = TEXT("UEAbilityKit");
		Category.Title = FText::FromString(TEXT("UE Ability Kit"));
		Category.Description = FText::FromString(TEXT("UAK 技能编辑器的分步引导"));
		Category.SortOrder = 100;
		IIntroTutorials::Get().RegisterCategory(Category);
	}
#endif

	GConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("UAK.Tutorial.Guided"),
		TEXT("启动交互引导：UAK.Tutorial.Guided <教程id|脚本路径>（读者自己动手，引擎判定进度）"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CmdGuided)));

	GConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("UAK.Tutorial.GuidedStop"),
		TEXT("结束当前交互引导"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CmdGuidedStop)));
}

void FUAKTutorialGuided::Unregister()
{
	for (IConsoleObject* Command : GConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Command);
	}
	GConsoleCommands.Reset();

	Get().Stop();
}
