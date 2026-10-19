#include "UAKAbilityInstance.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"
#include "UAKTrace.h"

void UAKAbilityInstance::SetTrace(UAKTrace* InTrace)
{
	Trace = InTrace;
	RootNodeIndex = Trace ? Trace->GetRootIndex() : INDEX_NONE;
}

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

	// 复合轨道（Sequence / Parallel / Repeat）在这里被展开成扁平调度表
	Asset->BuildExecutionPlan(Plan, /*bOnlyEnabledTracks=*/true);

	if (Plan.Num() == 0 && Asset->Length <= 0.f)
	{
		// 空技能：立刻结束，避免空转
		bFinished = true;
	}
}

bool UAKAbilityInstance::IsGated() const
{
	for (const FAKActiveTask& Active : ActiveTasks)
	{
		if (Active.bGate)
		{
			return true;
		}
	}
	return false;
}

void UAKAbilityInstance::Tick(float DeltaTime)
{
	if (bFinished || !Asset)
	{
		return;
	}

	const float Rate = FMath::Max(Asset->PlayRate, 0.01f);
	const float ScaledDelta = DeltaTime * Rate;

	// 门控：存在"冻结时间轴"的任务时，时间不前进（但持续任务仍会 Tick）
	if (!IsGated())
	{
		Context.CurrentTime += ScaledDelta;
	}

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
	while (PendingIndex < Plan.Num())
	{
		const FAKScheduledTask& Scheduled = Plan[PendingIndex];
		UAKAbilityTaskBase* Task = Scheduled.Task;

		if (!Task)
		{
			++PendingIndex;
			continue;
		}

		if (Scheduled.StartTime > Context.CurrentTime)
		{
			break; // 后面的更晚，停止本轮
		}

		++PendingIndex;

		// 分支结果：被 Branch 任务停用的轨道不参与本次运行
		if (Asset->Tracks.IsValidIndex(Scheduled.TrackIndex))
		{
			if (const UAKAbilityTrack* OwnerTrack = Asset->Tracks[Scheduled.TrackIndex])
			{
				if (Context.DisabledTracks.Contains(OwnerTrack->TrackName))
				{
					continue;
				}
			}
		}

		// 执行端过滤：ServerOnly 不在非权威端跑，ClientOnly 不在非本地端跑
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

		// 溯源：记录任务节点（未开启记录时 Trace 为 nullptr）
		const int32 NodeIndex = Trace
			? Trace->AddNode(RootNodeIndex, EAKTraceNodeKind::Task,
				Task->GetClass()->GetName().Replace(TEXT("UAKAbilityTask_"), TEXT("")), Scheduled.StartTime)
			: INDEX_NONE;

		Task->OnTaskStart(Context);

		const bool bSingleFrame = (Scheduled.EndTime <= Scheduled.StartTime);
		if (bSingleFrame)
		{
			// 单帧任务：进入即结束，不进入活跃列表
			Task->OnTaskEnd(Context, /*bInterrupted=*/false);
		}
		else
		{
			FAKActiveTask Active;
			Active.Task = Task;
			Active.EndTime = Scheduled.EndTime;
			Active.bNeedsTick = Task->bNeedsTick;
			Active.bGate = Task->bGateTimeline;
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
		const FAKActiveTask& Active = ActiveTasks[Index];
		if (!Active.Task)
		{
			ActiveTasks.RemoveAtSwap(Index);
			continue;
		}

		const bool bTimeUp = (Context.CurrentTime >= Active.EndTime);
		const bool bEarly = Active.Task->ShouldEndEarly(Context);

		if (bTimeUp || bEarly)
		{
			Active.Task->OnTaskEnd(Context, /*bInterrupted=*/false);

			// 门控任务结束时把状态写回，供下一帧判断是否恢复推进
			if (Active.TraceNodeIndex != INDEX_NONE && Trace)
			{
				Trace->AddDetail(Active.TraceNodeIndex, bEarly ? TEXT("条件满足，提前结束") : TEXT("到时结束"));
			}

			ActiveTasks.RemoveAtSwap(Index);
		}
	}
}

void UAKAbilityInstance::RestartLoop()
{
	// 循环：结束当前存活任务（视为正常结束），回到循环起点并重新武装调度表
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
	PendingIndex = Plan.Num();
	bFinished = true;

	OnFinished.ExecuteIfBound(bInterrupted);
}
