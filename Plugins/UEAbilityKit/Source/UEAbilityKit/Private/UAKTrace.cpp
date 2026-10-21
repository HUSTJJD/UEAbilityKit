#include "UAKTrace.h"

#include "UEAbilityKitModule.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	const TCHAR* TraceKindName(EAKTraceNodeKind Kind)
	{
		switch (Kind)
		{
		case EAKTraceNodeKind::Ability:		return TEXT("Ability");
		case EAKTraceNodeKind::Task:		return TEXT("Task");
		case EAKTraceNodeKind::Effect:		return TEXT("GE");
		case EAKTraceNodeKind::Cue:			return TEXT("Cue");
		case EAKTraceNodeKind::Target:		return TEXT("Target");
		case EAKTraceNodeKind::Attribute:	return TEXT("Attribute");
		}
		return TEXT("?");
	}

	/** 计算节点深度向上到根；ParentIndex 异常时以 0 兜底，避免死循环 */
	int32 NodeDepth(const TArray<FAKTraceNode>& Nodes, int32 Index)
	{
		int32 Depth = 0;
		int32 Cursor = Index;
		while (Nodes.IsValidIndex(Cursor) && Nodes[Cursor].ParentIndex != INDEX_NONE && Depth < 64)
		{
			++Depth;
			Cursor = Nodes[Cursor].ParentIndex;
		}
		return Depth;
	}
}

int32 UAKTrace::AddNode(int32 ParentIndex, EAKTraceNodeKind Kind, const FString& Label, float Time)
{
	FAKTraceNode Node;
	Node.ParentIndex = ParentIndex;
	Node.Kind = Kind;
	Node.Label = Label;
	Node.Time = Time;

	const int32 Index = Nodes.Add(MoveTemp(Node));
	if (Index == 0)
	{
		RootIndex = Index;
	}
	return Index;
}

void UAKTrace::AddDetail(int32 NodeIndex, const FString& Detail)
{
	if (Nodes.IsValidIndex(NodeIndex))
	{
		Nodes[NodeIndex].Details.Add(Detail);
	}
}

void UAKTrace::Reset()
{
	Nodes.Reset();
	RootIndex = INDEX_NONE;
}

FString UAKTrace::ToDebugString() const
{
	FString Out;
	Out.Reserve(Nodes.Num() * 64);

	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		const FAKTraceNode& Node = Nodes[i];
		const int32 Depth = NodeDepth(Nodes, i);

		Out += FString::ChrN(Depth * 2, TEXT(' '));
		Out += FString::Printf(TEXT("[%s] %s @ %.3fs"), TraceKindName(Node.Kind), *Node.Label, Node.Time);

		for (const FString& Detail : Node.Details)
		{
			Out += FString::Printf(TEXT("  {%s}"), *Detail);
		}
		Out += LINE_TERMINATOR;
	}
	return Out;
}

UAKTrace* UAKTraceSubsystem::BeginTrace(const FString& Label)
{
	if (!bRecordingEnabled)
	{
		return nullptr;
	}

	UAKTrace* Trace = NewObject<UAKTrace>(this);
	Trace->AddNode(INDEX_NONE, EAKTraceNodeKind::Ability, Label, 0.f);
	ActiveTraces.Add(Trace);
	return Trace;
}

void UAKTraceSubsystem::EndTrace(UAKTrace* Trace)
{
	if (!Trace)
	{
		return;
	}

	ActiveTraces.Remove(Trace);
	FinishedTraces.Add(Trace);

	// 只保留最近若干条，避免长时间运行后无限增长
	constexpr int32 MaxFinished = 32;
	while (FinishedTraces.Num() > MaxFinished)
	{
		FinishedTraces.RemoveAt(0);
	}
}

void UAKTraceSubsystem::ClearFinished()
{
	FinishedTraces.Reset();
}

namespace
{
	/** 收集当前所有真正会跑技能的世界（Game / PIE），调试命令只作用于它们 */
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

	void SetTraceRecording(bool bEnable)
	{
		int32 Count = 0;
		for (UWorld* World : GetGameWorlds())
		{
			if (UAKTraceSubsystem* TraceSubsystem = World->GetSubsystem<UAKTraceSubsystem>())
			{
				TraceSubsystem->bRecordingEnabled = bEnable;
				++Count;
			}
		}
		UE_LOG(LogUAK, Display, TEXT("UAK: 溯源记录已在 %d 个世界设为「%s」"), Count, bEnable ? TEXT("开") : TEXT("关"));
	}

	void CmdTraceEnable()
	{
		SetTraceRecording(true);
	}

	void CmdTraceDisable()
	{
		SetTraceRecording(false);
	}

	void CmdTraceClear()
	{
		for (UWorld* World : GetGameWorlds())
		{
			if (UAKTraceSubsystem* TraceSubsystem = World->GetSubsystem<UAKTraceSubsystem>())
			{
				TraceSubsystem->ClearFinished();
			}
		}
		UE_LOG(LogUAK, Display, TEXT("UAK: 已清空已完成的溯源记录"));
	}

	/** UAK.Trace.Dump [N]：导出最近 N 条（默认 1 条）已完成的溯源记录 */
	void CmdTraceDump(const TArray<FString>& Args, UWorld* /*TriggerWorld*/)
	{
		int32 MaxCount = 1;
		if (Args.Num() > 0)
		{
			MaxCount = FMath::Max(1, FCString::Atoi(*Args[0]));
		}

		int32 Printed = 0;
		for (UWorld* World : GetGameWorlds())
		{
			UAKTraceSubsystem* TraceSubsystem = World->GetSubsystem<UAKTraceSubsystem>();
			if (!TraceSubsystem)
			{
				continue;
			}

			const TArray<UAKTrace*>& Finished = TraceSubsystem->GetFinishedTraces();
			const int32 From = FMath::Max(0, Finished.Num() - MaxCount);
			for (int32 i = From; i < Finished.Num(); ++i)
			{
				if (!Finished[i])
				{
					continue;
				}
				UE_LOG(LogUAK, Display, TEXT("UAK Trace (%s) —— 第 %d 条：%s%s"),
					*World->GetName(), i - From + 1, LINE_TERMINATOR, *Finished[i]->ToDebugString());
				++Printed;
			}

			const TArray<UAKTrace*>& Active = TraceSubsystem->GetActiveTraces();
			for (const UAKTrace* Trace : Active)
			{
				if (!Trace)
				{
					continue;
				}
				UE_LOG(LogUAK, Display, TEXT("UAK Trace (%s) —— 进行中：%s%s"),
					*World->GetName(), LINE_TERMINATOR, *Trace->ToDebugString());
				++Printed;
			}
		}

		if (Printed == 0)
		{
			UE_LOG(LogUAK, Warning, TEXT("UAK: 没有溯源记录。先执行 UAK.Trace.Enable，再放一次技能。"));
		}
	}

	FAutoConsoleCommand GCmdTraceEnable(
		TEXT("UAK.Trace.Enable"),
		TEXT("开启 UAK 溯源记录（作用于所有 Game / PIE 世界）"),
		FConsoleCommandDelegate::CreateStatic(&CmdTraceEnable));

	FAutoConsoleCommand GCmdTraceDisable(
		TEXT("UAK.Trace.Disable"),
		TEXT("关闭 UAK 溯源记录（关闭时为零开销）"),
		FConsoleCommandDelegate::CreateStatic(&CmdTraceDisable));

	FAutoConsoleCommand GCmdTraceClear(
		TEXT("UAK.Trace.Clear"),
		TEXT("清空已完成的溯源记录"),
		FConsoleCommandDelegate::CreateStatic(&CmdTraceClear));

	FAutoConsoleCommandWithWorldAndArgs GCmdTraceDump(
		TEXT("UAK.Trace.Dump"),
		TEXT("导出溯源记录到日志：UAK.Trace.Dump [条数]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdTraceDump));
}
