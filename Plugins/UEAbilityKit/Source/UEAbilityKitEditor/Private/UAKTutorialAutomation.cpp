#include "UAKTutorialAutomation.h"

#include "SUAKTimeline.h"
#include "UEAbilityKitAssetEditor.h"
#include "UEAbilityKitModule.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityJsonSerializer.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"
#include "UAKTypes.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "ImageUtils.h"
#include "Layout/Geometry.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UnrealType.h"
#include "Widgets/SWidget.h"

#include "Slate/WidgetRenderer.h"   // FWidgetRenderer（UMG 模块，头文件在 UMG/Public/Slate 下）

DEFINE_LOG_CATEGORY_STATIC(LogUAKTutorial, Log, All);

namespace
{
	/** 动手前 / 截图前各等这么久，让 Slate 完成一次完整的 Tick + Paint */
	constexpr float SettleSeconds = 0.15f;
	/** 截图尺寸上限，避免 4K 屏上出一张几 MB 的图 */
	constexpr int32 MaxCaptureWidth = 2560;
	constexpr int32 MaxCaptureHeight = 1440;

	// ── 参数读取 ─────────────────────────────────────────────────────────
	FString ArgStr(const FUAKTutorialStep& Step, const TCHAR* Key, const FString& Default = FString())
	{
		FString Value;
		return Step.Args.IsValid() && Step.Args->TryGetStringField(Key, Value) ? Value : Default;
	}

	float ArgFloat(const FUAKTutorialStep& Step, const TCHAR* Key, float Default)
	{
		double Value = 0.0;
		return (Step.Args.IsValid() && Step.Args->TryGetNumberField(Key, Value)) ? static_cast<float>(Value) : Default;
	}

	int32 ArgInt(const FUAKTutorialStep& Step, const TCHAR* Key, int32 Default)
	{
		int32 Value = 0;
		return (Step.Args.IsValid() && Step.Args->TryGetNumberField(Key, Value)) ? Value : Default;
	}

	bool ArgBool(const FUAKTutorialStep& Step, const TCHAR* Key, bool Default)
	{
		bool Value = false;
		return (Step.Args.IsValid() && Step.Args->TryGetBoolField(Key, Value)) ? Value : Default;
	}

	// ── 资产 ─────────────────────────────────────────────────────────────
	/** 建（或重置）教程资产：教程每次录制都从同一起点开始，产物才可复现 */
	UAKAbilityAsset* CreateOrResetAsset(const FUAKTutorialAsset& Desc, FString& OutError)
	{
		const FString AssetName = FPackageName::GetShortName(Desc.Path);
		UPackage* Package = CreatePackage(*Desc.Path);
		if (!Package)
		{
			OutError = FString::Printf(TEXT("建不了包：%s"), *Desc.Path);
			return nullptr;
		}
		Package->FullyLoad();

		UAKAbilityAsset* Asset = FindObject<UAKAbilityAsset>(Package, *AssetName);
		if (!Asset)
		{
			Asset = NewObject<UAKAbilityAsset>(Package, UAKAbilityAsset::StaticClass(), *AssetName, RF_Public | RF_Standalone);
		}

		if (!Asset)
		{
			OutError = TEXT("建不了 UAKAbilityAsset");
			return nullptr;
		}

		Asset->Modify();
		Asset->Tracks.Empty();
		Asset->Markers.Empty();
		Asset->Length = Desc.Length;
		Asset->FrameRate = Desc.FrameRate;
		Asset->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Asset);
		return Asset;
	}

	UAKAbilityTrack* ResolveTrack(UAKAbilityAsset* Asset, const FUAKTutorialStep& Step, FString& OutError)
	{
		if (!Asset)
		{
			OutError = TEXT("当前没有资产");
			return nullptr;
		}

		const FString Name = ArgStr(Step, TEXT("track"));
		// args.track 既能给下标（0）也能给轨道名（Effect）
		if (!Name.IsEmpty() && FCString::IsNumeric(*Name))
		{
			const int32 Index = FCString::Atoi(*Name);
			return Asset->Tracks.IsValidIndex(Index) ? Asset->Tracks[Index] : nullptr;
		}
		if (ArgStr(Step, TEXT("track")).IsEmpty())
		{
			const int32 Index = ArgInt(Step, TEXT("trackIndex"), 0);
			return Asset->Tracks.IsValidIndex(Index) ? Asset->Tracks[Index] : nullptr;
		}

		for (UAKAbilityTrack* Track : Asset->Tracks)
		{
			if (Track && Track->TrackName.ToString().Equals(Name, ESearchCase::IgnoreCase))
			{
				return Track;
			}
		}
		OutError = FString::Printf(TEXT("找不到轨道 %s"), *Name);
		return nullptr;
	}

	UAKAbilityTaskBase* ResolveTask(UAKAbilityTrack* Track, const FUAKTutorialStep& Step, FString& OutError)
	{
		if (!Track)
		{
			OutError = TEXT("当前没有轨道");
			return nullptr;
		}
		const int32 Index = ArgInt(Step, TEXT("task"), ArgInt(Step, TEXT("taskIndex"), 0));
		if (!Track->Tasks.IsValidIndex(Index))
		{
			OutError = FString::Printf(TEXT("轨道上没有第 %d 个任务"), Index);
			return nullptr;
		}
		return Track->Tasks[Index];
	}

	/** 按 Json 值写属性：走 ImportText，任何 UPROPERTY 都通用（不必为每个 Task 写适配） */
	bool SetPropertyByName(UObject* Object, const FString& Name, const TSharedPtr<FJsonValue>& Value, FString& OutError)
	{
		if (!Object || !Value.IsValid())
		{
			return false;
		}

		FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), *Name);
		if (!Property)
		{
			OutError = FString::Printf(TEXT("%s 上没有属性 %s"), *Object->GetClass()->GetName(), *Name);
			return false;
		}

		FString Text;
		switch (Value->Type)
		{
		case EJson::Number:  Text = FString::SanitizeFloat(Value->AsNumber()); break;
		case EJson::Boolean: Text = Value->AsBool() ? TEXT("True") : TEXT("False"); break;
		default:             Value->TryGetString(Text); break;
		}

		Object->Modify();
		Property->ImportText_Direct(*Text, Property->ContainerPtrToValuePtr<void>(Object), Object, PPF_None);
		return true;
	}

	bool ApplyProps(UObject* Object, const FUAKTutorialStep& Step, FString& OutError)
	{
		const TSharedPtr<FJsonObject>* Props = nullptr;
		if (!Step.Args.IsValid() || !Step.Args->TryGetObjectField(TEXT("props"), Props) || !Props || !Props->IsValid())
		{
			return true;
		}

		bool bOk = true;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Props)->Values)
		{
			FString Error;
			if (!SetPropertyByName(Object, Pair.Key, Pair.Value, Error))
			{
				OutError = Error;
				bOk = false;
			}
		}
		return bOk;
	}

	/** 打开（或复用）某资产的编辑器 */
	TSharedPtr<FUEAbilityKitAssetEditor> OpenEditorFor(UAKAbilityAsset* Asset)
	{
		if (!Asset)
		{
			return nullptr;
		}

		TSharedPtr<FUEAbilityKitAssetEditor> Editor = FUEAbilityKitAssetEditor::FindEditorForAsset(Asset);
		if (Editor.IsValid())
		{
			return Editor;
		}

		if (GEditor)
		{
			if (UAssetEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				Subsystem->OpenEditorForAsset(Asset);
			}
		}
		return FUEAbilityKitAssetEditor::FindEditorForAsset(Asset);
	}

	// ── 截图 ─────────────────────────────────────────────────────────────
	/**
	 * 把任意 Slate 控件渲到 PNG。
	 *
	 * 为什么不用 FScreenshotRequest：它截的是视口，而教程要截的是面板（时间轴 / 血缘 / Inspector）。
	 * FWidgetRenderer 直接把控件画到 RenderTarget，跟窗口大小、屏幕分辨率都无关，
	 * 因此在 CI / 无窗口环境下也能稳定出图（前提是别用 -NullRHI）。
	 */
	bool RenderWidgetToPng(TSharedRef<SWidget> Widget, FIntPoint Size, const FString& Path)
	{
		UTextureRenderTarget2D* RenderTarget =
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, /*bUseSRGB=*/true);
		if (!RenderTarget)
		{
			return false;
		}

		TSharedRef<FWidgetRenderer> Renderer = MakeShared<FWidgetRenderer>(/*bUseGammaCorrection=*/true);
		Renderer->DrawWidget(RenderTarget, Widget, FVector2D(Size.X, Size.Y), 0.f);
		FlushRenderingCommands();

		bool bOk = false;
		if (TUniquePtr<FArchive> Ar = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*Path)))
		{
			bOk = FImageUtils::ExportRenderTarget2DAsPNG(RenderTarget, *Ar);
			Ar->Close();
		}

		RenderTarget->ReleaseResource();
		return bOk;
	}

	void EmitMarker(const TCHAR* Tutorial, const FString& Step, const TCHAR* Status, const FString& Extra)
	{
		UE_LOG(LogUAKTutorial, Display, TEXT("UAK_TUTORIAL %s %s %s%s"),
			Tutorial, *Step, Status,
			Extra.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" %s"), *Extra));
	}

	TArray<IConsoleObject*> GRegisteredCommands;
	bool GCaptureEnabled = true;
}

// ─────────────────────────────────────────────────────────────────────────
// 执行器
// ─────────────────────────────────────────────────────────────────────────
FUAKTutorialRunner& FUAKTutorialRunner::Get()
{
	static FUAKTutorialRunner Instance;
	return Instance;
}

bool FUAKTutorialRunner::Start(const FUAKTutorialScript& InScript, bool bInCapture)
{
	if (bRunning)
	{
		UE_LOG(LogUAKTutorial, Warning, TEXT("[UAK] 正在跑 %s，先 Stop 再换一条"), *CurrentId);
		return false;
	}

	Script = InScript;
	CurrentId = Script.Id;
	bCapture = bInCapture;
	StepIndex = INDEX_NONE;
	Results.Reset();
	Phase = EPhase::Settle;
	SettleTimer = 0.f;
	StepElapsed = 0.f;
	WaitRemaining = 0.f;
	CurrentAsset = nullptr;

	OutputRoot = FUAKTutorialAutomation::GetMediaRoot();
	TutorialDir = OutputRoot / Script.Id;
	RunJsonPath = TutorialDir / TEXT("run.json");
	IFileManager::Get().MakeDirectory(*TutorialDir, true);

	bRunning = true;
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FUAKTutorialRunner::Tick), 0.f);

	UE_LOG(LogUAKTutorial, Display, TEXT("[UAK] 开始录制教程 %s（%d 步，采集=%d）"),
		*CurrentId, Script.Steps.Num(), bCapture ? 1 : 0);

	// 从第 0 步开始（Advance 会走 ++StepIndex → BeginStep）
	Advance();
	return true;
}

bool FUAKTutorialRunner::Enqueue(const TArray<FUAKTutorialScript>& Scripts, bool bInCapture, bool bInExitWhenDone)
{
	if (Scripts.Num() == 0)
	{
		return false;
	}

	Queued = Scripts;
	bRequestExitWhenDone = bInExitWhenDone;

	FUAKTutorialScript First = Queued[0];
	Queued.RemoveAt(0);
	return Start(First, bInCapture);
}

void FUAKTutorialRunner::Abort()
{
	if (bRunning)
	{
		bAborted = true;
		FinishRun(TEXT("ABORT"));
	}
}

bool FUAKTutorialRunner::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return false;
	}

	const float Delta = FMath::Clamp(DeltaTime, 0.f, 0.5f);
	StepElapsed += Delta;

	switch (Phase)
	{
	case EPhase::Settle:
		SettleTimer += Delta;
		if (SettleTimer >= SettleSeconds)
		{
			Phase = EPhase::Act;
		}
		break;

	case EPhase::Act:
		ExecuteCurrentStep();
		Phase = EPhase::PostSettle;
		SettleTimer = 0.f;
		break;

	case EPhase::PostSettle:
		SettleTimer += Delta;
		if (WaitRemaining > 0.f)
		{
			WaitRemaining -= Delta;
		}
		if (SettleTimer < SettleSeconds || WaitRemaining > 0.f)
		{
			break;
		}
		Phase = Script.Steps.IsValidIndex(StepIndex) && Script.Steps[StepIndex].Capture.bShot
			? EPhase::Shot
			: EPhase::Record;
		SettleTimer = 0.f;
		RecordTimer = 0.f;
		RecordFrame = 0;
		RecordTotal = 0;
		break;

	case EPhase::Shot:
		CaptureShot();
		Phase = EPhase::Record;
		RecordTimer = 0.f;
		RecordFrame = 0;
		RecordTotal = 0;
		break;

	case EPhase::Record:
		TickRecord(Delta);
		break;
	}

	return true;
}

void FUAKTutorialRunner::BeginStep()
{
	FUAKTutorialStepResult Result;
	if (Script.Steps.IsValidIndex(StepIndex))
	{
		const FUAKTutorialStep& Step = Script.Steps[StepIndex];
		Result.Id = Step.Id;
		Result.Title = Step.Title;
		Result.Say = Step.Say;
		Result.Do = Step.Do;
		Result.Expect = Step.Expect;
		Result.Action = UAKTutorialActionToString(Step.Action);
	}
	Results.Add(MoveTemp(Result));

	StepElapsed = 0.f;
	SettleTimer = 0.f;
	WaitRemaining = 0.f;
}

void FUAKTutorialRunner::ExecuteCurrentStep()
{
	if (!Script.Steps.IsValidIndex(StepIndex))
	{
		return;
	}

	FUAKTutorialStep& Step = Script.Steps[StepIndex];
	FUAKTutorialStepResult& Result = Results.Last();

	FString Error;
	bool bOk = true;

	switch (Step.Action)
	{
	case EUAKTutorialAction::Note:
		break;

	case EUAKTutorialAction::CreateAsset:
	{
		CurrentAsset = CreateOrResetAsset(Script.Asset, Error);
		bOk = CurrentAsset.IsValid();
		break;
	}

	case EUAKTutorialAction::OpenAsset:
	{
		if (!CurrentAsset.IsValid())
		{
			CurrentAsset = CreateOrResetAsset(Script.Asset, Error);
		}
		bOk = OpenEditorFor(CurrentAsset.Get()).IsValid();
		if (!bOk && Error.IsEmpty())
		{
			Error = TEXT("打开资产编辑器失败");
		}
		break;
	}

	case EUAKTutorialAction::CloseAsset:
	{
		if (CurrentAsset.IsValid() && GEditor)
		{
			if (UAssetEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				Subsystem->CloseAllEditorsForAsset(CurrentAsset.Get());
			}
		}
		break;
	}

	case EUAKTutorialAction::FocusTab:
	{
		TSharedPtr<FUEAbilityKitAssetEditor> Editor = OpenEditorFor(CurrentAsset.Get());
		if (Editor.IsValid())
		{
			Editor->AutomationFocusTab(FUEAbilityKitAssetEditor::GetTabIdForName(ArgStr(Step, TEXT("tab"), TEXT("Timeline"))));
		}
		else
		{
			bOk = false;
			Error = TEXT("没有打开的资产编辑器");
		}
		break;
	}

	case EUAKTutorialAction::AddTrack:
	{
		UAKAbilityAsset* Asset = CurrentAsset.Get();
		if (!Asset)
		{
			bOk = false;
			Error = TEXT("没有资产");
			break;
		}

		Asset->Modify();
		UAKAbilityTrack* Track = NewObject<UAKAbilityTrack>(Asset);
		Track->TrackName = FName(*ArgStr(Step, TEXT("name"), TEXT("Track")));
		Track->TrackType = static_cast<EAKTrackType>(ArgInt(Step, TEXT("type"), static_cast<int32>(EAKTrackType::Custom)));
		Track->CompositeMode = EAKTrackCompositeMode::Timeline;
		Asset->Tracks.Add(Track);
		Asset->MarkPackageDirty();
		break;
	}

	case EUAKTutorialAction::AddTask:
	{
		UAKAbilityTrack* Track = ResolveTrack(CurrentAsset.Get(), Step, Error);
		if (!Track)
		{
			bOk = false;
			break;
		}

		const FString TypeName = ArgStr(Step, TEXT("type"));
		UAKAbilityTaskBase* Task = UAKAbilityJsonSerializer::CreateTaskByTypeName(Track, TypeName);
		if (!Task)
		{
			bOk = false;
			Error = FString::Printf(TEXT("创建任务失败：%s"), *TypeName);
			break;
		}

		Track->Modify();
		Task->StartTime = ArgFloat(Step, TEXT("startTime"), 0.f);
		Task->Duration = ArgFloat(Step, TEXT("duration"), 0.f);
		const FString TaskId = ArgStr(Step, TEXT("taskId"));
		if (!TaskId.IsEmpty())
		{
			Task->TaskId = FName(*TaskId);
		}
		Track->Tasks.Add(Task);
		bOk = ApplyProps(Task, Step, Error);
		break;
	}

	case EUAKTutorialAction::SelectTask:
	{
		UAKAbilityTrack* Track = ResolveTrack(CurrentAsset.Get(), Step, Error);
		UAKAbilityTaskBase* Task = ResolveTask(Track, Step, Error);
		if (!Task)
		{
			bOk = false;
			break;
		}

		if (TSharedPtr<FUEAbilityKitAssetEditor> Editor = OpenEditorFor(CurrentAsset.Get()))
		{
			Editor->AutomationSelectTask(Task);
		}
		break;
	}

	case EUAKTutorialAction::SelectTrack:
	{
		UAKAbilityTrack* Track = ResolveTrack(CurrentAsset.Get(), Step, Error);
		if (!Track)
		{
			bOk = false;
			break;
		}

		if (TSharedPtr<FUEAbilityKitAssetEditor> Editor = OpenEditorFor(CurrentAsset.Get()))
		{
			Editor->AutomationSelectTrack(CurrentAsset->Tracks.IndexOfByKey(Track));
		}
		break;
	}

	case EUAKTutorialAction::SetTrackMode:
	{
		UAKAbilityTrack* Track = ResolveTrack(CurrentAsset.Get(), Step, Error);
		if (!Track)
		{
			bOk = false;
			break;
		}

		Track->Modify();
		const FString Mode = ArgStr(Step, TEXT("mode"), TEXT("Timeline"));
		if (Mode.Equals(TEXT("Sequence"), ESearchCase::IgnoreCase))
		{
			Track->CompositeMode = EAKTrackCompositeMode::Sequence;
		}
		else if (Mode.Equals(TEXT("Parallel"), ESearchCase::IgnoreCase))
		{
			Track->CompositeMode = EAKTrackCompositeMode::Parallel;
		}
		else
		{
			Track->CompositeMode = EAKTrackCompositeMode::Timeline;
		}
		Track->RepeatCount = FMath::Max(ArgInt(Step, TEXT("repeatCount"), 1), 1);
		Track->RepeatInterval = ArgFloat(Step, TEXT("repeatInterval"), 0.f);
		Track->StartDelay = ArgFloat(Step, TEXT("startDelay"), 0.f);
		Track->bEnabled = ArgBool(Step, TEXT("enabled"), Track->bEnabled);
		break;
	}

	case EUAKTutorialAction::SetTaskProp:
	{
		UAKAbilityTrack* Track = ResolveTrack(CurrentAsset.Get(), Step, Error);
		UAKAbilityTaskBase* Task = ResolveTask(Track, Step, Error);
		if (!Task)
		{
			bOk = false;
			break;
		}

		const FString Name = ArgStr(Step, TEXT("property"));
		bOk = SetPropertyByName(Task, Name, MakeShared<FJsonValueString>(ArgStr(Step, TEXT("value"))), Error);
		break;
	}

	case EUAKTutorialAction::SetAssetProp:
	{
		UAKAbilityAsset* Asset = CurrentAsset.Get();
		if (!Asset)
		{
			bOk = false;
			Error = TEXT("没有资产");
			break;
		}
		bOk = SetPropertyByName(Asset, ArgStr(Step, TEXT("property")), MakeShared<FJsonValueString>(ArgStr(Step, TEXT("value"))), Error);
		break;
	}

	case EUAKTutorialAction::SetCursor:
	{
		if (TSharedPtr<FUEAbilityKitAssetEditor> Editor = OpenEditorFor(CurrentAsset.Get()))
		{
			Editor->AutomationSetCursorTime(ArgFloat(Step, TEXT("time"), 0.f));
		}
		else
		{
			bOk = false;
			Error = TEXT("没有打开的资产编辑器");
		}
		break;
	}

	case EUAKTutorialAction::Play:
	case EUAKTutorialAction::Pause:
	case EUAKTutorialAction::Stop:
	{
		if (TSharedPtr<FUEAbilityKitAssetEditor> Editor = OpenEditorFor(CurrentAsset.Get()))
		{
			if (Step.Action == EUAKTutorialAction::Play)
			{
				Editor->AutomationPlay();
			}
			else if (Step.Action == EUAKTutorialAction::Pause)
			{
				Editor->AutomationPause();
			}
			else
			{
				Editor->AutomationStop();
			}
		}
		else
		{
			bOk = false;
			Error = TEXT("没有打开的资产编辑器");
		}
		break;
	}

	case EUAKTutorialAction::StepFrames:
	{
		if (TSharedPtr<FUEAbilityKitAssetEditor> Editor = OpenEditorFor(CurrentAsset.Get()))
		{
			Editor->AutomationStepFrames(ArgInt(Step, TEXT("frames"), 1));
		}
		else
		{
			bOk = false;
			Error = TEXT("没有打开的资产编辑器");
		}
		break;
	}

	case EUAKTutorialAction::ToggleFrames:
	{
		if (TSharedPtr<FUEAbilityKitAssetEditor> Editor = OpenEditorFor(CurrentAsset.Get()))
		{
			Editor->AutomationSetShowFrames(ArgBool(Step, TEXT("on"), true));
		}
		break;
	}

	case EUAKTutorialAction::Validate:
	{
		UAKAbilityAsset* Asset = CurrentAsset.Get();
		if (!Asset)
		{
			bOk = false;
			Error = TEXT("没有资产");
			break;
		}

		FAKValidationResult Validation;
		Asset->Validate(Validation);
		Result.Errors = Validation.NumErrors();
		Result.Warnings = Validation.NumWarnings();

		for (const FAKValidationIssue& Issue : Validation.Issues)
		{
			if (Issue.Severity == EAKValidationSeverity::Error || Issue.Severity == EAKValidationSeverity::Warning)
			{
				Result.Notes.Add(Issue.Message.ToString());
			}
		}

		if (Result.Errors != Step.ExpectErrors)
		{
			bOk = false;
			Error = FString::Printf(TEXT("错误数 %d，期望 %d"), Result.Errors, Step.ExpectErrors);
		}
		if (Step.ExpectWarnings >= 0 && Result.Warnings != Step.ExpectWarnings)
		{
			bOk = false;
			Error = FString::Printf(TEXT("警告数 %d，期望 %d"), Result.Warnings, Step.ExpectWarnings);
		}
		break;
	}

	case EUAKTutorialAction::ExportJson:
	{
		UAKAbilityAsset* Asset = CurrentAsset.Get();
		if (!Asset)
		{
			bOk = false;
			Error = TEXT("没有资产");
			break;
		}

		const FString RelPath = ArgStr(Step, TEXT("path"), FString::Printf(TEXT("%s.json"), *Step.Id));
		const FString Target = TutorialDir / TEXT("export") / RelPath;
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target), true);
		bOk = UAKAbilityJsonSerializer::ExportToFile(Asset, Target);
		if (bOk)
		{
			Result.Notes.Add(FString::Printf(TEXT("导出 %s"), *RelPath));
		}
		else
		{
			Error = TEXT("导出 Json 失败");
		}
		break;
	}

	case EUAKTutorialAction::ImportJson:
	{
		UAKAbilityAsset* Asset = CurrentAsset.Get();
		if (!Asset)
		{
			bOk = false;
			Error = TEXT("没有资产");
			break;
		}

		const FString Source = ArgStr(Step, TEXT("path"));
		FString Json;
		bOk = FFileHelper::LoadFileToString(Json, *Source) && UAKAbilityJsonSerializer::ImportFromJson(Asset, Json);
		if (!bOk)
		{
			Error = FString::Printf(TEXT("导入 Json 失败：%s"), *Source);
		}
		break;
	}

	case EUAKTutorialAction::Wait:
		WaitRemaining = ArgFloat(Step, TEXT("seconds"), 1.f);
		break;

	case EUAKTutorialAction::Exec:
	{
		const FString Command = ArgStr(Step, TEXT("cmd"));
		if (Command.IsEmpty())
		{
			bOk = false;
			Error = TEXT("Exec 步骤没有 cmd");
			break;
		}
		if (GEngine)
		{
			GEngine->Exec(nullptr, *Command);
			Result.Notes.Add(Command);
		}
		break;
	}

	default:
		break;
	}

	// 每步都记一遍"当前的资产状态"，文档里 reader 可以逐条对照
	if (const UAKAbilityAsset* Asset = CurrentAsset.Get())
	{
		Result.Tracks = Asset->Tracks.Num();
		int32 TaskCount = 0;
		for (const UAKAbilityTrack* Track : Asset->Tracks)
		{
			TaskCount += Track ? Track->Tasks.Num() : 0;
		}
		Result.Tasks = TaskCount;
	}
	if (TSharedPtr<FUEAbilityKitAssetEditor> Editor = FUEAbilityKitAssetEditor::FindEditorForAsset(CurrentAsset.Get()))
	{
		Editor->AutomationRefresh();
		Result.Cursor = Editor->GetCursorTimePublic();
	}

	if (!bOk)
	{
		Result.Status = TEXT("FAIL");
		if (!Error.IsEmpty())
		{
			Result.Notes.Add(Error);
		}
	}
	else
	{
		Result.Status = TEXT("OK");
	}

	EmitMarker(*CurrentId, Result.Id, *Result.Status,
		FString::Printf(TEXT("tracks=%d tasks=%d errors=%d"), Result.Tracks, Result.Tasks, Result.Errors));
}

void FUAKTutorialRunner::CaptureShot()
{
	if (!bCapture || !Script.Steps.IsValidIndex(StepIndex))
	{
		return;
	}

	const FUAKTutorialStep& Step = Script.Steps[StepIndex];
	FUAKTutorialStepResult& Result = Results.Last();

	const FString FileName = FString::Printf(TEXT("step_%02d_%s.png"), StepIndex + 1, *Step.Id);
	FIntPoint Size = FIntPoint::ZeroValue;
	if (CaptureRegion(Step.Capture.Region, FileName, Size))
	{
		Result.Shot = FileName;
	}
	else
	{
		Result.Notes.Add(TEXT("截图失败（没有可采的控件，或 -NullRHI 无像素）"));
	}
}

void FUAKTutorialRunner::TickRecord(float DeltaTime)
{
	if (!Script.Steps.IsValidIndex(StepIndex))
	{
		Advance();
		return;
	}

	const FUAKTutorialStep& Step = Script.Steps[StepIndex];
	FUAKTutorialStepResult& Result = Results.Last();

	if (!bCapture || !Step.Capture.bRecord || Step.Capture.Seconds <= 0.f)
	{
		FinishStep(Result.Status == TEXT("FAIL") ? TEXT("FAIL") : TEXT("OK"));
		Advance();
		return;
	}

	if (RecordTotal == 0)
	{
		RecordTotal = FMath::Max(1, FMath::CeilToInt(Step.Capture.Seconds * Step.Capture.Fps));
		Result.FramesDir = FString::Printf(TEXT("step_%02d_%s/frames"), StepIndex + 1, *Step.Id);
	}

	RecordTimer += DeltaTime;
	const float Interval = 1.f / FMath::Max(Step.Capture.Fps, 1.f);
	while (RecordTimer >= Interval && RecordFrame < RecordTotal)
	{
		RecordTimer -= Interval;
		const FString RelPath = FString::Printf(TEXT("%s/frame_%05d.png"), *Result.FramesDir, RecordFrame + 1);
		FIntPoint Size = FIntPoint::ZeroValue;
		if (CaptureRegion(Step.Capture.Region, RelPath, Size))
		{
			++Result.Frames;
		}
		++RecordFrame;
	}

	if (RecordFrame >= RecordTotal)
	{
		FinishStep(Result.Status == TEXT("FAIL") ? TEXT("FAIL") : TEXT("OK"));
		Advance();
	}
}

bool FUAKTutorialRunner::CaptureRegion(EUAKTutorialRegion Region, const FString& RelativePath, FIntPoint& OutSize)
{
	TSharedPtr<FUEAbilityKitAssetEditor> Editor = FUEAbilityKitAssetEditor::FindEditorForAsset(CurrentAsset.Get());
	if (!Editor.IsValid())
	{
		return false;
	}

	TSharedPtr<SWidget> Widget = Editor->GetWidgetForRegion(Region);
	if (!Widget.IsValid())
	{
		return false;
	}

	const FVector2D Local = Widget->GetCachedGeometry().GetLocalSize();
	FIntPoint Size(
		FMath::Clamp(FMath::CeilToInt(Local.X), 1, MaxCaptureWidth),
		FMath::Clamp(FMath::CeilToInt(Local.Y), 1, MaxCaptureHeight));
	if (Size.X < 8 || Size.Y < 8)
	{
		// 控件还没被布局过（Tab 没打开过）：给一个可读的默认尺寸
		Size = FIntPoint(1280, 720);
	}
	OutSize = Size;

	const FString Target = TutorialDir / RelativePath;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target), true);
	return RenderWidgetToPng(Widget.ToSharedRef(), Size, Target);
}

void FUAKTutorialRunner::FinishStep(const TCHAR* Status)
{
	if (Results.Num() > 0)
	{
		Results.Last().Status = Status;
		Results.Last().Duration = StepElapsed;
	}
}

void FUAKTutorialRunner::Advance()
{
	++StepIndex;
	if (!Script.Steps.IsValidIndex(StepIndex))
	{
		FinishRun(TEXT("OK"));
		return;
	}

	Phase = EPhase::Settle;
	SettleTimer = 0.f;
	BeginStep();
}

void FUAKTutorialRunner::FinishRun(const TCHAR* Status)
{
	if (!bRunning)
	{
		return;
	}

	// 只有被中断时，当前这一步才是"没跑完"；正常跑完时各步已有自己的结论
	if (bAborted)
	{
		FinishStep(Status);
	}
	bAborted = false;

	WriteRunJson();

	bRunning = false;
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();

	int32 Failed = 0;
	for (const FUAKTutorialStepResult& Result : Results)
	{
		if (Result.Status != TEXT("OK"))
		{
			++Failed;
		}
	}

	UE_LOG(LogUAKTutorial, Display, TEXT("UAK_TUTORIAL_RUN %s %s steps=%d failed=%d path=%s"),
		*CurrentId, Status, Results.Num(), Failed, *RunJsonPath);

	const bool bExit = bRequestExitWhenDone;

	if (Queued.Num() > 0)
	{
		FUAKTutorialScript Next = Queued[0];
		Queued.RemoveAt(0);
		Start(Next, bCapture);
		return;
	}

	bRequestExitWhenDone = false;
	if (bExit)
	{
		UE_LOG(LogUAKTutorial, Display, TEXT("[UAK] 教程录制完毕，请求退出进程"));
		FPlatformMisc::RequestExit(false);
	}
}

bool FUAKTutorialRunner::WriteRunJson()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("tutorial"), Script.Id);
	Root->SetStringField(TEXT("title"), Script.Title);
	Root->SetStringField(TEXT("summary"), Script.Summary);
	Root->SetStringField(TEXT("level"), Script.Level);
	Root->SetStringField(TEXT("duration"), Script.DurationText);
	Root->SetStringField(TEXT("source"), Script.SourcePath);
	Root->SetStringField(TEXT("asset"), Script.Asset.Path);
	Root->SetNumberField(TEXT("fps"), Script.Fps);
	Root->SetStringField(TEXT("stamp"), FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")));
	Root->SetStringField(TEXT("engine"), FApp::GetBuildVersion());
	Root->SetBoolField(TEXT("captured"), bCapture);

	TArray<TSharedPtr<FJsonValue>> Prereq;
	for (const FString& Item : Script.Prerequisites)
	{
		Prereq.Add(MakeShared<FJsonValueString>(Item));
	}
	Root->SetArrayField(TEXT("prerequisites"), Prereq);

	TArray<TSharedPtr<FJsonValue>> Checks;
	for (const FString& Item : Script.Checks)
	{
		Checks.Add(MakeShared<FJsonValueString>(Item));
	}
	Root->SetArrayField(TEXT("checks"), Checks);

	TArray<TSharedPtr<FJsonValue>> Steps;
	for (const FUAKTutorialStepResult& Result : Results)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("id"), Result.Id);
		Obj->SetStringField(TEXT("title"), Result.Title);
		Obj->SetStringField(TEXT("say"), Result.Say);
		Obj->SetStringField(TEXT("do"), Result.Do);
		Obj->SetStringField(TEXT("expect"), Result.Expect);
		Obj->SetStringField(TEXT("action"), Result.Action);
		Obj->SetStringField(TEXT("status"), Result.Status);
		Obj->SetNumberField(TEXT("duration"), Result.Duration);
		Obj->SetStringField(TEXT("shot"), Result.Shot);
		Obj->SetStringField(TEXT("framesDir"), Result.FramesDir);
		Obj->SetNumberField(TEXT("frames"), Result.Frames);
		Obj->SetNumberField(TEXT("tracks"), Result.Tracks);
		Obj->SetNumberField(TEXT("tasks"), Result.Tasks);
		Obj->SetNumberField(TEXT("cursor"), Result.Cursor);
		Obj->SetNumberField(TEXT("errors"), Result.Errors);
		Obj->SetNumberField(TEXT("warnings"), Result.Warnings);

		TArray<TSharedPtr<FJsonValue>> Notes;
		for (const FString& Note : Result.Notes)
		{
			Notes.Add(MakeShared<FJsonValueString>(Note));
		}
		Obj->SetArrayField(TEXT("notes"), Notes);
		Steps.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Root->SetArrayField(TEXT("steps"), Steps);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	return FFileHelper::SaveStringToFile(Json, *RunJsonPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

// ─────────────────────────────────────────────────────────────────────────
// 命令
// ─────────────────────────────────────────────────────────────────────────
FString FUAKTutorialAutomation::GetScriptDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tutorials"));
}

FString FUAKTutorialAutomation::GetMediaRoot()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("UAKAutomation") / TEXT("Tutorials"));
}

namespace
{
	TArray<FString> FindScripts(const FString& Dir)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.json")), true, false);
		Files.Sort();
		return Files;
	}

	bool LoadScriptByPathOrId(const FString& Query, FUAKTutorialScript& OutScript)
	{
		const FString Dir = FUAKTutorialAutomation::GetScriptDir();

		FString Error;
		FString Path = Query;
		if (FPaths::IsRelative(Path) && IFileManager::Get().FileExists(*(Dir / (Query + TEXT(".json")))))
		{
			Path = Dir / (Query + TEXT(".json"));
		}

		if (FUAKTutorialScript::LoadFromFile(Path, OutScript, Error))
		{
			return true;
		}
		UE_LOG(LogUAKTutorial, Error, TEXT("[UAK] %s"), *Error);
		return false;
	}

	void CmdRun(const TArray<FString>& Args)
	{
		const FString Query = Args.Num() > 0 ? Args[0] : FString();
		if (Query.IsEmpty())
		{
			UE_LOG(LogUAKTutorial, Error, TEXT("[UAK] 用法：UAK.Tutorial.Run <脚本路径|教程 id>"));
			return;
		}

		FUAKTutorialScript Script;
		if (LoadScriptByPathOrId(Query, Script))
		{
			FUAKTutorialRunner::Get().Start(Script, GCaptureEnabled);
		}
	}

	void CmdRunAll(const TArray<FString>& Args)
	{
		// 开关（以 - 开头，如 -Quit）不是目录：早期实现把 Args[0] 直接当目录，
		// 于是 "UAK.Tutorial.RunAll -Quit" 会去找一个叫 -Quit 的目录。
		FString Dir;
		for (const FString& Arg : Args)
		{
			if (!Arg.StartsWith(TEXT("-")) && Dir.IsEmpty())
			{
				Dir = Arg;
			}
		}
		if (Dir.IsEmpty())
		{
			Dir = FUAKTutorialAutomation::GetScriptDir();
		}

		TArray<FUAKTutorialScript> Scripts;
		for (const FString& File : FindScripts(Dir))
		{
			FUAKTutorialScript Script;
			FString Error;
			if (FUAKTutorialScript::LoadFromFile(Dir / File, Script, Error))
			{
				Scripts.Add(MoveTemp(Script));
			}
			else
			{
				UE_LOG(LogUAKTutorial, Error, TEXT("[UAK] %s"), *Error);
			}
		}

		if (Scripts.Num() == 0)
		{
			UE_LOG(LogUAKTutorial, Error, TEXT("[UAK] %s 下没有教程脚本"), *Dir);
			return;
		}

		// -Quit：全部跑完自动退出，方便脚本无值守跑
		bool bQuit = false;
		for (const FString& Arg : Args)
		{
			if (Arg.Equals(TEXT("-Quit"), ESearchCase::IgnoreCase))
			{
				bQuit = true;
			}
		}

		FUAKTutorialRunner::Get().Enqueue(Scripts, GCaptureEnabled, bQuit);
	}

	void CmdStop(const TArray<FString>& Args)
	{
		FUAKTutorialRunner::Get().Abort();
	}

	void CmdCapture(const TArray<FString>& Args)
	{
		GCaptureEnabled = !(Args.Num() > 0 && Args[0] == TEXT("0"));
		UE_LOG(LogUAKTutorial, Display, TEXT("[UAK] 教程素材采集：%s"), GCaptureEnabled ? TEXT("开") : TEXT("关"));
	}

	void CmdList(const TArray<FString>& Args)
	{
		const FString Dir = Args.Num() > 0 ? Args[0] : FUAKTutorialAutomation::GetScriptDir();
		for (const FString& File : FindScripts(Dir))
		{
			FUAKTutorialScript Script;
			FString Error;
			if (FUAKTutorialScript::LoadFromFile(Dir / File, Script, Error))
			{
				UE_LOG(LogUAKTutorial, Display, TEXT("[UAK] %-28s %-3d 步  %s"),
					*Script.Id, Script.Steps.Num(), *Script.Title);
			}
		}
	}
}

void FUAKTutorialAutomation::Register()
{
	if (!GRegisteredCommands.IsEmpty())
	{
		return;
	}

	IConsoleManager& Console = IConsoleManager::Get();
	GRegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("UAK.Tutorial.Run"),
		TEXT("录制一条教程：UAK.Tutorial.Run <脚本路径|教程 id>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CmdRun)));

	GRegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("UAK.Tutorial.RunAll"),
		TEXT("录制目录下全部教程：UAK.Tutorial.RunAll [目录] [-Quit]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CmdRunAll)));

	GRegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("UAK.Tutorial.List"),
		TEXT("列出可用教程：UAK.Tutorial.List [目录]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CmdList)));

	GRegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("UAK.Tutorial.Stop"),
		TEXT("中断当前教程录制"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CmdStop)));

	GRegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("UAK.Tutorial.Capture"),
		TEXT("开关素材采集：UAK.Tutorial.Capture 0|1（关掉=只验证步骤）"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CmdCapture)));
}

void FUAKTutorialAutomation::Unregister()
{
	IConsoleManager& Console = IConsoleManager::Get();
	for (IConsoleObject* Object : GRegisteredCommands)
	{
		Console.UnregisterConsoleObject(Object);
	}
	GRegisteredCommands.Reset();
}
