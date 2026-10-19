#include "UAKAbilityInstance.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"
#include "UAKTrace.h"

namespace
{
	/** 轨道优先级：数值越小越先结束（先停外在表现，最后停主时序） */
	int32 TrackPriorityFor(EAKTrackType Type)
	{
		switch (Type)
		{
		case EAKTrackType::Presentation:	return 0;
		case EAKTrackType::Motion:			return 1;
		case EAKTrackType::Query:			return 2;
		case EAKTrackType::Effect:			return 3;
		case EAKTrackType::Custom:			return 4;
		case EAKTrackType::Rule:			return 5;
		case EAKTrackType::Main:
		default:							return 6;
		}
	}
}

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
	StartOrderCounter = 0;
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

bool UAKAbilityInstance::IsTrackDisabled(int32 TrackIndex) const
{
	if (!Asset || !Asset->Tracks.IsValidIndex(TrackIndex))
	{
		return false;
	}

	const UAKAbilityTrack* Track = Asset->Tracks[TrackIndex];
	return Track && Context.DisabledTracks.Contains(Track->TrackName);
}

bool UAKAbilityInstance::ShouldRunOnThisEnd(const UAKAbilityTaskBase* Task) const
{
	if (!Task)
	{
		return false;
	}

	const bool bSkipServerOnly = (Task->NetMode == EAKTaskNetMode::ServerOnly) && !Context.bIsAuthority;
	const bool bSkipClientOnly = (Task->NetMode == EAKTaskNetMode::ClientOnly) && !Context.bIsLocallyControlled;
	return !bSkipServerOnly && !bSkipClientOnly;
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
	EndTasksOnDisabledTracks();
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
		if (IsTrackDisabled(Scheduled.TrackIndex))
		{
			continue;
		}

		// 执行端过滤
		if (!ShouldRunOnThisEnd(Task))
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
			Task->OnTaskEnd(Context, /*bInterrupted=*/false);
		}
		else
		{
			FAKActiveTask Active;
			Active.Task = Task;
			Active.EndTime = Scheduled.EndTime;
			Active.bNeedsTick = Task->bNeedsTick;
			Active.bGate = Task->bGateTimeline;
			Active.StartOrder = ++StartOrderCounter;
			Active.TrackIndex = Scheduled.TrackIndex;
			Active.TraceNodeIndex = NodeIndex;
			ActiveTasks.Add(Active);
		}
	}
}

/** 轨道被 Branch 停用后，取消该轨道上仍在运行的任务（视为取消，不是技能中断） */
void UAKAbilityInstance::EndTasksOnDisabledTracks()
{
	for (int32 Index = ActiveTasks.Num() - 1; Index >= 0; --Index)
	{
		FAKActiveTask& Active = ActiveTasks[Index];
		if (!Active.Task || !IsTrackDisabled(Active.TrackIndex))
		{
			continue;
		}

		Active.Task->OnTaskEnd(Context, /*bInterrupted=*/true);

		if (Trace && Active.TraceNodeIndex != INDEX_NONE)
		{
			Trace->AddDetail(Active.TraceNodeIndex, TEXT("所属轨道被 Branch 停用而取消"));
		}

		ActiveTasks.RemoveAt(Index);
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
		FAKActiveTask& Active = ActiveTasks[Index];
		if (!Active.Task)
		{
			ActiveTasks.RemoveAt(Index);
			continue;
		}

		const bool bTimeUp = (Context.CurrentTime >= Active.EndTime);
		const bool bEarly = Active.Task->ShouldEndEarly(Context);

		if (bTimeUp || bEarly)
		{
			Active.Task->OnTaskEnd(Context, /*bInterrupted=*/false);

			if (Active.TraceNodeIndex != INDEX_NONE && Trace)
			{
				Trace->AddDetail(Active.TraceNodeIndex, bEarly ? TEXT("条件满足，提前结束") : TEXT("到时结束"));
			}

			// 用 RemoveAt 而非 RemoveAtSwap：保持启动顺序，中断顺序才有意义
			ActiveTasks.RemoveAt(Index);
		}
	}
}

void UAKAbilityInstance::SortForInterrupt(TArray<FAKActiveTask>& InOut) const
{
	const EAKInterruptOrder Order = Asset ? Asset->InterruptOrder : EAKInterruptOrder::ReverseStartOrder;

	if (Order == EAKInterruptOrder::StartOrder)
	{
		InOut.Sort([](const FAKActiveTask& A, const FAKActiveTask& B)
		{
			return A.StartOrder < B.StartOrder;
		});
		return;
	}

	if (Order == EAKInterruptOrder::TrackPriority && Asset)
	{
		InOut.Sort([this](const FAKActiveTask& A, const FAKActiveTask& B)
		{
			const int32 PriorityA = Asset->Tracks.IsValidIndex(A.TrackIndex) && Asset->Tracks[A.TrackIndex]
				? TrackPriorityFor(Asset->Tracks[A.TrackIndex]->TrackType) : 99;
			const int32 PriorityB = Asset->Tracks.IsValidIndex(B.TrackIndex) && Asset->Tracks[B.TrackIndex]
				? TrackPriorityFor(Asset->Tracks[B.TrackIndex]->TrackType) : 99;

			if (PriorityA != PriorityB)
			{
				return PriorityA < PriorityB;
			}
			return A.StartOrder < B.StartOrder;
		});
		return;
	}

	// ReverseStartOrder（默认）：后启动的先结束，等价于栈式回退
	InOut.Sort([](const FAKActiveTask& A, const FAKActiveTask& B)
	{
		return A.StartOrder > B.StartOrder;
	});
}

void UAKAbilityInstance::RunGuaranteedPendingTasks()
{
	for (int32 Index = PendingIndex; Index < Plan.Num(); ++Index)
	{
		const FAKScheduledTask& Scheduled = Plan[Index];
		UAKAbilityTaskBase* Task = Scheduled.Task;

		if (!Task || !Task->bAlwaysExecute)
		{
			continue;
		}

		// 轨道已被 Branch 明确关闭，或本端不该执行：跳过
		if (IsTrackDisabled(Scheduled.TrackIndex) || !ShouldRunOnThisEnd(Task))
		{
			continue;
		}

		Task->OnTaskStart(Context);
		Task->OnTaskEnd(Context, /*bInterrupted=*/false);

		if (Trace)
		{
			const int32 NodeIndex = Trace->AddNode(RootNodeIndex, EAKTraceNodeKind::Task,
				Task->GetClass()->GetName().Replace(TEXT("UAKAbilityTask_"), TEXT("")), Context.CurrentTime);
			Trace->AddDetail(NodeIndex, TEXT("技能被中断，但标记为保证执行，补执行一次"));
		}
	}
}

void UAKAbilityInstance::RestartLoop()
{
	TArray<FAKActiveTask> Ordered = ActiveTasks;
	SortForInterrupt(Ordered);

	for (const FAKActiveTask& Active : Ordered)
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

	// 1) 按资产的中断顺序结束存活任务
	TArray<FAKActiveTask> Ordered = ActiveTasks;
	SortForInterrupt(Ordered);

	for (const FAKActiveTask& Active : Ordered)
	{
		if (!Active.Task)
		{
			continue;
		}

		// "保证执行"的任务按正常方式结束，避免表现突兀
		const bool bTaskInterrupted = bInterrupted && !Active.Task->bAlwaysExecute;
		Active.Task->OnTaskEnd(Context, bTaskInterrupted);

		if (Trace && Active.TraceNodeIndex != INDEX_NONE)
		{
			Trace->AddDetail(Active.TraceNodeIndex,
				bTaskInterrupted ? TEXT("被中断") : TEXT("标记为保证执行，正常结束"));
		}
	}

	ActiveTasks.Reset();

	// 2) 被中断时，补执行那些"保证执行"但还没轮到的任务
	if (bInterrupted)
	{
		RunGuaranteedPendingTasks();
	}

	Context.ScratchPads.Reset();
	Context.DisabledTracks.Reset();
	PendingIndex = Plan.Num();
	bFinished = true;

	OnFinished.ExecuteIfBound(bInterrupted);
}
