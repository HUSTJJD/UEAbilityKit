#include "UAKDebugSubsystem.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTaskBase.h"
#include "UAKTrace.h"
#include "UEAbilityKitModule.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Stats/Stats.h"

namespace
{
	bool GAKDebugDrawEnabled = false;

	/** 任务的显示名：优先 TaskId（同类任务多次出现时能区分），否则去掉类名前缀 */
	FString TaskLabel(const UAKAbilityTaskBase* Task)
	{
		if (!Task)
		{
			return TEXT("<null>");
		}

		if (!Task->TaskId.IsNone())
		{
			return Task->TaskId.ToString();
		}

		static const FString Prefix(TEXT("UAKAbilityTask_"));
		FString Name = Task->GetClass()->GetName();
		if (Name.StartsWith(Prefix))
		{
			Name = Name.Mid(Prefix.Len());
		}
		return Name;
	}

	TArray<UWorld*> GetGameWorlds()
	{
		TArray<UWorld*> Out;
		if (!GEngine)
		{
			return Out;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
			{
				if (UWorld* World = Context.World())
				{
					Out.Add(World);
				}
			}
		}
		return Out;
	}
}

bool UAKDebugSubsystem::IsDebugDrawEnabled()
{
	return GAKDebugDrawEnabled;
}

void UAKDebugSubsystem::SetDebugDrawEnabled(bool bEnabled)
{
	GAKDebugDrawEnabled = bEnabled;
}

void UAKDebugSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ActiveInstances.Reset();
}

void UAKDebugSubsystem::Deinitialize()
{
	ActiveInstances.Reset();
	Super::Deinitialize();
}

bool UAKDebugSubsystem::IsTickable() const
{
	// 只在开关打开时进入 Tick 队列，关闭时零开销
	return GAKDebugDrawEnabled && Super::IsTickable();
}

TStatId UAKDebugSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAKDebugSubsystem, STATGROUP_Tickables);
}

void UAKDebugSubsystem::RegisterInstance(UAKAbilityInstance* Instance)
{
	if (Instance)
	{
		ActiveInstances.AddUnique(Instance);
	}
}

void UAKDebugSubsystem::UnregisterInstance(UAKAbilityInstance* Instance)
{
	ActiveInstances.Remove(Instance);
}

TArray<FAKDebugEntry> UAKDebugSubsystem::BuildEntries()
{
	// 顺手清理已经结束却没来得及注销的实例（例如技能所在 Actor 被销毁）
	ActiveInstances.RemoveAll([](const UAKAbilityInstance* Instance)
	{
		return !IsValid(Instance) || Instance->IsFinished();
	});

	CachedEntries.Reset(ActiveInstances.Num());

	for (const UAKAbilityInstance* Instance : ActiveInstances)
	{
		if (!Instance)
		{
			continue;
		}

		FAKDebugEntry Entry;
		Entry.Owner = Instance->GetContext().Owner.Get();
		Entry.OwnerName = Entry.Owner.IsValid() ? Entry.Owner->GetActorNameOrLabel() : TEXT("<已销毁>");
		Entry.AbilityName = Instance->GetAsset() ? Instance->GetAsset()->GetName() : TEXT("<无资产>");
		Entry.CurrentTime = Instance->GetCurrentTime();
		Entry.Length = Instance->GetLength();
		Entry.LoopCount = Instance->GetLoopCount();
		Entry.bGated = Instance->IsGated();
		Entry.Trace = Instance->GetTrace();

		for (const FAKActiveTask& Active : Instance->GetActiveTasks())
		{
			const float Remain = FMath::Max(0.f, Active.EndTime - Entry.CurrentTime);
			Entry.ActiveTasks.Add(FString::Printf(TEXT("%s (剩 %.2fs)%s"),
				*TaskLabel(Active.Task), Remain, Active.bGate ? TEXT(" [门控]") : TEXT("")));
		}

		CachedEntries.Add(MoveTemp(Entry));
	}

	return CachedEntries;
}

void UAKDebugSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!GAKDebugDrawEnabled)
	{
		return;
	}

	DrawWorldOverlay();
}

void UAKDebugSubsystem::DrawWorldOverlay()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const FAKDebugEntry& Entry : BuildEntries())
	{
		if (!Entry.Owner.IsValid())
		{
			continue;
		}

		FString Text = FString::Printf(TEXT("%s | %s %.2f/%.2fs%s"),
			*Entry.OwnerName, *Entry.AbilityName, Entry.CurrentTime, Entry.Length,
			Entry.bGated ? TEXT(" | 已冻结") : TEXT(""));

		if (Entry.LoopCount > 0)
		{
			Text += FString::Printf(TEXT(" | 第 %d 次循环"), Entry.LoopCount + 1);
		}

		for (const FString& TaskText : Entry.ActiveTasks)
		{
			Text += LINE_TERMINATOR + TaskText;
		}

		const FVector Location = Entry.Owner->GetActorLocation() + FVector(0.f, 0.f, 120.f);
		const FColor Color = Entry.bGated ? FColor::Orange : FColor::Green;

		// Duration = 0：只画一帧，由本系统的 Tick 负责持续刷新
		DrawDebugString(World, Location, Text, nullptr, Color, /*Duration=*/0.f, /*bDrawShadow=*/true);
	}
}

namespace
{
	void CmdDebugEnable()
	{
		UAKDebugSubsystem::SetDebugDrawEnabled(true);

		// 调试时几乎总是同时需要溯源：顺手打开，并在日志里说明
		int32 Count = 0;
		for (UWorld* World : GetGameWorlds())
		{
			if (UAKTraceSubsystem* TraceSubsystem = World->GetSubsystem<UAKTraceSubsystem>())
			{
				TraceSubsystem->bRecordingEnabled = true;
				++Count;
			}
		}

		UE_LOG(LogUAK, Display, TEXT("UAK: 调试器已开启（世界内标注 + %d 个世界的溯源记录）"), Count);
	}

	void CmdDebugDisable()
	{
		UAKDebugSubsystem::SetDebugDrawEnabled(false);
		UE_LOG(LogUAK, Display, TEXT("UAK: 调试器已关闭（记录开关不受影响，用 UAK.Trace.Disable 单独关）"));
	}

	void CmdDebugSummary()
	{
		int32 Total = 0;
		for (UWorld* World : GetGameWorlds())
		{
			UAKDebugSubsystem* DebugSubsystem = World->GetSubsystem<UAKDebugSubsystem>();
			if (!DebugSubsystem)
			{
				continue;
			}

			const TArray<FAKDebugEntry> Entries = DebugSubsystem->BuildEntries();
			if (Entries.Num() == 0)
			{
				UE_LOG(LogUAK, Display, TEXT("UAK Debug (%s)：没有正在运行的技能"), *World->GetName());
				continue;
			}

			for (const FAKDebugEntry& Entry : Entries)
			{
				UE_LOG(LogUAK, Display, TEXT("UAK Debug (%s)：%s | %s %.2f/%.2fs%s | 任务 %d 个"),
					*World->GetName(), *Entry.OwnerName, *Entry.AbilityName,
					Entry.CurrentTime, Entry.Length, Entry.bGated ? TEXT(" [已冻结]") : TEXT(""),
					Entry.ActiveTasks.Num());

				for (const FString& TaskText : Entry.ActiveTasks)
				{
					UE_LOG(LogUAK, Display, TEXT("    └ %s"), *TaskText);
				}
				++Total;
			}
		}

		if (Total == 0)
		{
			UE_LOG(LogUAK, Warning, TEXT("UAK: 当前没有活跃技能。确认已进 PIE 且技能真的被激活。"));
		}
	}

	void CmdDebugHelp()
	{
		UE_LOG(LogUAK, Display, TEXT("UAK 调试器命令："));
		UE_LOG(LogUAK, Display, TEXT("  UAK.Debug.Enable    开启世界内标注 + 溯源记录"));
		UE_LOG(LogUAK, Display, TEXT("  UAK.Debug.Disable   关闭世界内标注"));
		UE_LOG(LogUAK, Display, TEXT("  UAK.Debug.Summary   把当前活跃技能打到日志"));
		UE_LOG(LogUAK, Display, TEXT("  UAK.Debug.Help      显示本帮助"));
		UE_LOG(LogUAK, Display, TEXT("  UAK.Trace.Enable / Dump [N] / Clear / Disable   溯源记录（见 docs/examples/06-live-trace-checklist.md）"));
	}

	FAutoConsoleCommand GCmdDebugEnable(
		TEXT("UAK.Debug.Enable"),
		TEXT("开启 UAK 调试器：世界内标注 + 溯源记录"),
		FConsoleCommandDelegate::CreateStatic(&CmdDebugEnable));

	FAutoConsoleCommand GCmdDebugDisable(
		TEXT("UAK.Debug.Disable"),
		TEXT("关闭 UAK 调试器的世界内标注"),
		FConsoleCommandDelegate::CreateStatic(&CmdDebugDisable));

	FAutoConsoleCommand GCmdDebugSummary(
		TEXT("UAK.Debug.Summary"),
		TEXT("把当前正在运行的 UAK 技能打到日志"),
		FConsoleCommandDelegate::CreateStatic(&CmdDebugSummary));

	FAutoConsoleCommand GCmdDebugHelp(
		TEXT("UAK.Debug.Help"),
		TEXT("显示 UAK 调试器命令帮助"),
		FConsoleCommandDelegate::CreateStatic(&CmdDebugHelp));
}
