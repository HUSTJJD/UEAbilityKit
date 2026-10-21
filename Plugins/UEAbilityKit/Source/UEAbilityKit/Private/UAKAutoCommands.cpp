#include "UAKAutoCommands.h"

#include "UEAbilityKitModule.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UnrealClient.h"   // FScreenshotRequest

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"   // TFieldIterator / FStructProperty（dump 属性集）

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	// ── 输出 ───────────────────────────────────────────────────────────
	void EmitMarker(const TCHAR* Action, const FString& Name, const TCHAR* Status, const FString& Extra = FString())
	{
		UE_LOG(LogUAK, Display, TEXT("UAK_E2E %s %s %s%s"),
			Action, *Name, Status,
			Extra.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" %s"), *Extra));
	}

	FString ScreenShotPath(const FString& ShotName)
	{
		return FPaths::ScreenShotDir() / (ShotName + TEXT(".png"));
	}

	// ── 世界：Standalone Game / PIE / 独立 DS 都算"正在跑的世界" ────────
	UWorld* GetPlayWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	// ── 快照（GAS 状态）────────────────────────────────────────────────
	void WriteSnapshot(UWorld* World, const FString& Role)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("role"), Role);
		Root->SetStringField(TEXT("map"), World->GetMapName());
		Root->SetNumberField(TEXT("time"), World->GetTimeSeconds());
		Root->SetNumberField(TEXT("netMode"), static_cast<double>(World->GetNetMode()));

		TArray<TSharedPtr<FJsonValue>> Actors;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor)
			{
				continue;
			}

			const IAbilitySystemInterface* AsInterface = Cast<IAbilitySystemInterface>(Actor);
			if (!AsInterface)
			{
				continue;
			}

			UAbilitySystemComponent* ASC = AsInterface->GetAbilitySystemComponent();
			if (!ASC)
			{
				continue;
			}

			TSharedRef<FJsonObject> Attributes = MakeShared<FJsonObject>();
			for (UAttributeSet* Set : ASC->GetSpawnedAttributes())
			{
				if (!Set)
				{
					continue;
				}

				// 不写死属性名：遍历属性集里的 FGameplayAttributeData，
				// 任何项目的属性集都能被同一段代码 dump 出来。
				for (TFieldIterator<FStructProperty> PropIt(Set->GetClass()); PropIt; ++PropIt)
				{
					FStructProperty* Prop = *PropIt;
					if (!Prop || Prop->Struct != FGameplayAttributeData::StaticStruct())
					{
						continue;
					}

					const FGameplayAttributeData* Data = Prop->ContainerPtrToValuePtr<FGameplayAttributeData>(Set);
					if (Data)
					{
						Attributes->SetNumberField(Prop->GetName(), Data->GetCurrentValue());
					}
				}
			}

			TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("name"), Actor->GetName());
			Entry->SetNumberField(TEXT("netRole"), static_cast<double>(Actor->GetLocalRole()));
			Entry->SetBoolField(TEXT("authority"), Actor->HasAuthority());
			Entry->SetObjectField(TEXT("attributes"), Attributes);
			Actors.Add(MakeShared<FJsonValueObject>(Entry));
		}
		Root->SetArrayField(TEXT("actors"), Actors);

		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root, Writer);

		const FString Dir = FUAKAutoCommands::GetMediaRoot() / TEXT("Net");
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = Dir / (Role + TEXT(".json"));

		const bool bOk = FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		EmitMarker(TEXT("Snapshot"), Role, bOk ? TEXT("OK") : TEXT("FAIL"),
			FString::Printf(TEXT("path=%s actors=%d"), *Path, Actors.Num()));
	}

	void SnapshotTick(FString Role)
	{
		if (UWorld* World = GetPlayWorld())
		{
			WriteSnapshot(World, Role);
		}
	}

	// ── 逐帧截图 ───────────────────────────────────────────────────────
	struct FRecordState
	{
		bool bActive = false;
		bool bSingleShot = false;
		FString Name;
		FString OutDir;
		int32 Total = 0;
		int32 Index = 0;
		int32 Stored = 0;
		int32 WaitTicks = 0;
		bool bShotPending = false;
		FTimerHandle Handle;
	};
	FRecordState GRecord;

	void StopRecord(bool bFinished)
	{
		if (GRecord.bActive && GRecord.Handle.IsValid())
		{
			if (UWorld* World = GetPlayWorld())
			{
				World->GetTimerManager().ClearTimer(GRecord.Handle);
			}
		}

		GRecord.bActive = false;
		if (bFinished)
		{
			EmitMarker(TEXT("Record"), GRecord.Name, GRecord.Stored > 0 ? TEXT("OK") : TEXT("FAIL"),
				FString::Printf(TEXT("frames=%d dir=%s"), GRecord.Stored, *GRecord.OutDir));
		}
	}

	void RecordTick()
	{
		if (!GRecord.bActive)
		{
			return;
		}

		const FString ShotName = FString::Printf(TEXT("UAKRec_%s_%05d"), *GRecord.Name, GRecord.Index);

		if (!GRecord.bShotPending)
		{
			FScreenshotRequest::RequestScreenshot(ShotName, false, false);
			GRecord.bShotPending = true;
			GRecord.WaitTicks = 0;
			return;
		}

		const FString Source = ScreenShotPath(ShotName);
		if (!IFileManager::Get().FileExists(*Source))
		{
			// DS 进程 / NullRHI 永远等不到像素，别把进程挂死
			if (++GRecord.WaitTicks > 120)
			{
				GRecord.bShotPending = false;
				++GRecord.Index;
			}
			return;
		}

		const FString Target = GRecord.bSingleShot
			? GRecord.OutDir / TEXT("shot.png")
			: GRecord.OutDir / TEXT("frames") / FString::Printf(TEXT("frame_%05d.png"), GRecord.Stored);

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target), true);
		if (IFileManager::Get().Move(*Target, *Source) == 0)
		{
			// Move 失败（跨卷等）：退回 Copy + Delete
			IFileManager::Get().Copy(*Target, *Source);
			IFileManager::Get().Delete(*Source);
		}

		++GRecord.Stored;
		++GRecord.Index;
		GRecord.bShotPending = false;

		if (GRecord.Index >= GRecord.Total)
		{
			StopRecord(true);
		}
	}

	void StartCapture(const FString& Name, int32 Frames, float Fps, bool bSingleShot)
	{
		UWorld* World = GetPlayWorld();
		if (!World)
		{
			EmitMarker(TEXT("Record"), Name, TEXT("FAIL"), TEXT("reason=no-world"));
			return;
		}

		if (World->GetNetMode() == NM_DedicatedServer)
		{
			// DS 没有渲染目标，截图没有意义；同步正确性靠 Snapshot 比对
			EmitMarker(TEXT("Record"), Name, TEXT("SKIP"), TEXT("reason=dedicated-server"));
			return;
		}

		GRecord = FRecordState();
		GRecord.bActive = true;
		GRecord.bSingleShot = bSingleShot;
		GRecord.Name = Name;
		GRecord.OutDir = FUAKAutoCommands::GetMediaRoot() / Name;
		GRecord.Total = FMath::Max(Frames, 1);
		GRecord.Stored = 0;
		GRecord.Index = 0;

		IFileManager::Get().MakeDirectory(*GRecord.OutDir, true);

		FTimerDelegate Delegate = FTimerDelegate::CreateStatic(&RecordTick);
		World->GetTimerManager().SetTimer(GRecord.Handle, Delegate, FMath::Max(1.f / Fps, 0.001f), true);
	}

	// ── 命令实现 ───────────────────────────────────────────────────────
	/** 在本地玩家的 ASC 上激活技能；失败时把原因写进 OutReason */
	bool ActivateAbilityOnLocalPawn(const FString& ClassPath, FString& OutReason)
	{
		if (ClassPath.IsEmpty())
		{
			OutReason = TEXT("missing-class");
			return false;
		}

		UWorld* World = GetPlayWorld();
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;

		const IAbilitySystemInterface* AsInterface = Pawn ? Cast<IAbilitySystemInterface>(Pawn) : nullptr;
		UAbilitySystemComponent* ASC = AsInterface ? AsInterface->GetAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			OutReason = TEXT("no-local-asc");
			return false;
		}

		UClass* AbilityClass = LoadClass<UGameplayAbility>(nullptr, *ClassPath);
		if (!AbilityClass)
		{
			OutReason = TEXT("class-not-found");
			return false;
		}

		if (!ASC->TryActivateAbilityByClass(AbilityClass))
		{
			OutReason = TEXT("activation-refused");
			return false;
		}

		return true;
	}

	/** 定时重复激活：客户端连进来时 Pawn 还没生成，靠周期重试覆盖"连上之后"的时刻 */
	void ActivateTick(FString ClassPath)
	{
		FString Reason;
		if (ActivateAbilityOnLocalPawn(ClassPath, Reason))
		{
			EmitMarker(TEXT("ActivateAbility"), ClassPath, TEXT("OK"));
		}
		else
		{
			UE_LOG(LogUAK, Verbose, TEXT("[UAK] 定时激活未成功：%s（%s）"), *ClassPath, *Reason);
		}
	}

	void CmdActivateAbility(const TArray<FString>& Args)
	{
		const FString ClassPath = Args.Num() > 0 ? Args[0] : FString();
		const float Period = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;

		FString Reason;
		const bool bOk = ActivateAbilityOnLocalPawn(ClassPath, Reason);
		EmitMarker(TEXT("ActivateAbility"), ClassPath.IsEmpty() ? TEXT("(empty)") : ClassPath,
			bOk ? TEXT("OK") : TEXT("FAIL"), bOk ? FString() : FString::Printf(TEXT("reason=%s"), *Reason));

		if (Period > 0.f)
		{
			UWorld* World = GetPlayWorld();
			if (World)
			{
				FTimerDelegate Delegate = FTimerDelegate::CreateStatic(&ActivateTick, ClassPath);
				FTimerHandle Handle;
				World->GetTimerManager().SetTimer(Handle, Delegate, Period, true);
			}
		}
	}

	void CmdSnapshot(const TArray<FString>& Args)
	{
		const FString Role = Args.Num() > 0 ? Args[0] : TEXT("Unknown");
		const float Period = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;

		UWorld* World = GetPlayWorld();
		if (!World)
		{
			EmitMarker(TEXT("Snapshot"), Role, TEXT("FAIL"), TEXT("reason=no-world"));
			return;
		}

		WriteSnapshot(World, Role);

		if (Period > 0.f)
		{
			// 周期写：进程的最后一个文件就是"结束时刻的状态"，
			// 脚本 kill 进程前不需要再想办法通知它补一次快照。
			FTimerDelegate Delegate = FTimerDelegate::CreateStatic(&SnapshotTick, Role);
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, Delegate, Period, true);
			EmitMarker(TEXT("SnapshotTimer"), Role, TEXT("OK"), FString::Printf(TEXT("period=%.2f"), Period));
		}
	}

	void CmdRecord(const TArray<FString>& Args)
	{
		const FString Name = Args.Num() > 0 ? Args[0] : TEXT("UAK.Auto");
		const int32 Frames = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 60;
		const float Fps = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 30.f;
		StartCapture(Name, Frames, Fps, /*bSingleShot=*/false);
	}

	void CmdScreenshot(const TArray<FString>& Args)
	{
		const FString Name = Args.Num() > 0 ? Args[0] : TEXT("UAK.Auto");
		StartCapture(Name, 1, 30.f, /*bSingleShot=*/true);
	}

	TArray<IConsoleObject*> GRegisteredCommands;
}

FString FUAKAutoCommands::GetMediaRoot()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("UAKAutomation"));
}

void FUAKAutoCommands::Register()
{
	if (!GRegisteredCommands.IsEmpty())
	{
		return;
	}

	IConsoleManager& Console = IConsoleManager::Get();
	GRegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("UAK.Auto.ActivateAbility"),
		TEXT("在本地玩家的 ASC 上激活技能：UAK.Auto.ActivateAbility <ClassPath> [Period]（Period>0 时周期重试）"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CmdActivateAbility)));

	GRegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("UAK.Auto.Snapshot"),
		TEXT("导出当前 GAS 状态到 Saved/UAKAutomation/Net/<Role>.json：UAK.Auto.Snapshot <Role> [Period]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CmdSnapshot)));

	GRegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("UAK.Auto.Record"),
		TEXT("逐帧截图（录屏素材）：UAK.Auto.Record <Name> [Frames] [Fps]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CmdRecord)));

	GRegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("UAK.Auto.Screenshot"),
		TEXT("单张截图：UAK.Auto.Screenshot <Name>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CmdScreenshot)));
}

void FUAKAutoCommands::Unregister()
{
	IConsoleManager& Console = IConsoleManager::Get();
	for (IConsoleObject* Object : GRegisteredCommands)
	{
		Console.UnregisterConsoleObject(Object);
	}
	GRegisteredCommands.Reset();
}
