#include "UAKAbilityInstance.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"
#include "UAKTrace.h"

void UAKAbilityInstance::Initialize(UAKAbilityAsset* InAsset, const FAKAbilityContext& InContext)
{
	Asset = InAsset;
	Context = InContext;
	PendingIndex = 0;
	LoopCount = 0;
	bFinished = false;
	ActiveTasks.Reset();

	if (!Asset)
	{
		bFinished = true;
		return;
	}

	Context.PlayRate = Asset->PlayRate;

	if (Trace)
	{
		RootNodeIndex = Trace->GetRootIndex();
	}

	Asset->CollectTasks(PendingTasks, /*bOnlyEnabledTracks=*/true);
	PendingTasks.Sort([](const UAKAbilityTaskBase& A, const UAKAbilityTaskBase& B)
	{
		return A.StartTime < B.StartTime;
	});

	if (PendingTasks.Num() == 0 && Asset->Length <= 0.f)
	{
		// 空技能：立刻结束，避免空转
		bFinished = true;
	}
}

void UAKAbilityInstance::SetTrace(UAKTrace* InTrace)
{
	Trace = InTrace;
	RootNodeIndex = Trace ? Trace->GetRootIndex() : INDEX_NONE;
}

void UAKAbilityInstance::Tick(float DeltaTime)
{
	if (bFinished || !Asset)
	{
		return;
	}

	const float Rate = FMath::Max(Asset->PlayRate, 0.01f);
	const float ScaledDelta = DeltaTime * Rate;
	Context.CurrentTime += ScaledDelta;

	StartDueTasks();
	TickActiveTasks(ScaledDelta);
	EndDueTasks();

	if (Context.CurrentTime >= Asset->Length)
	{
		const bool bCanLoop = Asset->bLoop && (Asset->MaxLoops <= 0 || (LoopCount + 1) < Asset->MaxLoops);
		if (bCanLoop)
		{
			RestartLoop();
		}
		else
		{
			Finish(/*bInterrupted=*/false);
		}
	}
}

void UAKAbilityInstance::StartDueTasks()
{
	while (PendingIndex < PendingTasks.Num())
	{
		UAKAbilityTaskBase* Task = PendingTasks[PendingIndex];
		if (!Task)
		{
			++PendingIndex;
			continue;
		}

		if (Task->StartTime > Context.CurrentTime)
		{
			break; // 后面的任务起始时间更晚，停止本轮
		}

		++PendingIndex;

		// 执行端过滤：ServerOnly 的任务不在非权威端执行，ClientOnly 的任务不在非本地端执行。
		// 注意这不是网络同步——UAK 不自建同步，只是避免任务在不该跑的一端空跑。
		// 分支结果：被 Branch 任务停用的轨道不参与本次运行
		if (const UAKAbilityTrack* OwnerTrack = Cast<UAKAbilityTrack>(Task->GetOuter()))
		{
			if (Context.DisabledTracks.Contains(OwnerTrack->TrackName))
			{
				continue;
			}
		}

		const bool bSkipServerOnly = (Task->NetMode == EAKTaskNetMode::ServerOnly) && !Context.bIsAuthority;
		const bool bSkipClientOnly = (Task->NetMode == EAKTaskNetMode::ClientOnly) && !Context.bIsLocallyControlled;
		if (bSkipServerOnly || bSkipClientOnly)
		{
			continue;
		}

		if (UObject* ScratchPad = Task->CreateScratchPad(Context))
		{
			Context.ScratchPads.Add(Task, ScratchPad);
		}

		Task->OnTaskStart(Context);

		// 溯源：记录任务节点（未开启记录时 Trace 为 nullptr，此处是空判断，无额外开销）
		const int32 NodeIndex = Trace
			? Trace->AddNode(RootNodeIndex, EAKTraceNodeKind::Task,
				Task->GetClass()->GetName().Replace(TEXT("UAKAbilityTask_"), TEXT("")), Task->StartTime)
			: INDEX_NONE;

		if (Task->IsSingleFrame())
		{
			// 单帧任务：进入即结束，不进入活跃列表
			Task->OnTaskEnd(Context, /*bInterrupted=*/false);
		}
		else
		{
			FAKActiveTask Active;
			Active.Task = Task;
			Active.EndTime = Task->GetEndTime();
			Active.bNeedsTick = Task->bNeedsTick;
			Active.TraceNodeIndex = NodeIndex;
			ActiveTasks.Add(Active);
		}
	}
}

void UAKAbilityInstance::TickActiveTasks(float ScaledDeltaTime)
{
	for (const FAKActiveTask& Active : ActiveTasks)
	{
		if (Active.Task && Active.bNeedsTick)
		{
			Active.Task->OnTaskTick(Context, ScaledDeltaTime);
		}
	}
}

void UAKAbilityInstance::EndDueTasks()
{
	for (int32 Index = ActiveTasks.Num() - 1; Index >= 0; --Index)
	{
		if (Context.CurrentTime >= ActiveTasks[Index].EndTime)
		{
			if (UAKAbilityTaskBase* Task = ActiveTasks[Index].Task)
			{
				Task->OnTaskEnd(Context, /*bInterrupted=*/false);
			}
			ActiveTasks.RemoveAtSwap(Index);
		}
	}
}

void UAKAbilityInstance::RestartLoop()
{
	// 循环：结束当前存活任务（视为正常结束），回到循环起点并重新武装任务队列
	for (const FAKActiveTask& Active : ActiveTasks)
	{
		if (Active.Task)
		{
			Active.Task->OnTaskEnd(Context, /*bInterrupted=*/false);
		}
	}
	ActiveTasks.Reset();
	Context.ScratchPads.Reset();
	PendingIndex = 0;
	Context.CurrentTime = FMath::Max(0.f, Asset->LoopStartTime);
	++LoopCount;
}

void UAKAbilityInstance::Finish(bool bInterrupted)
{
	if (bFinished)
	{
		return;
	}

	for (const FAKActiveTask& Active : ActiveTasks)
	{
		if (Active.Task)
		{
			Active.Task->OnTaskEnd(Context, bInterrupted);
		}
	}

	ActiveTasks.Reset();
	Context.ScratchPads.Reset();
	PendingIndex = PendingTasks.Num();
	bFinished = true;

	OnFinished.ExecuteIfBound(bInterrupted);
}
