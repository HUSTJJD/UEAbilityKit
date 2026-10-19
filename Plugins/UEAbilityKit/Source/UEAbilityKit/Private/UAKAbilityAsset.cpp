#include "UAKAbilityAsset.h"

#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"

#define LOCTEXT_NAMESPACE "UAKAbilityAsset"

void UAKAbilityAsset::CollectTasks(TArray<UAKAbilityTaskBase*>& OutTasks, bool bOnlyEnabledTracks) const
{
	OutTasks.Reset();

	for (const UAKAbilityTrack* Track : Tracks)
	{
		if (!Track || (bOnlyEnabledTracks && !Track->bEnabled))
		{
			continue;
		}

		for (UAKAbilityTaskBase* Task : Track->Tasks)
		{
			if (Task)
			{
				OutTasks.Add(Task);
			}
		}
	}
}

void UAKAbilityAsset::BuildExecutionPlan(TArray<FAKScheduledTask>& OutPlan, bool bOnlyEnabledTracks) const
{
	OutPlan.Reset();

	for (int32 TrackIndex = 0; TrackIndex < Tracks.Num(); ++TrackIndex)
	{
		const UAKAbilityTrack* Track = Tracks[TrackIndex];
		if (!Track || (bOnlyEnabledTracks && !Track->bEnabled))
		{
			continue;
		}

		const bool bIsTimelineMode = (Track->CompositeMode == EAKTrackCompositeMode::Timeline);
		const int32 Repeats = bIsTimelineMode ? 1 : FMath::Max(1, Track->RepeatCount);
		const float Interval = FMath::Max(0.f, Track->RepeatInterval);
		const float StartDelay = FMath::Max(0.f, Track->StartDelay);

		// 计算一轮之内每个任务的相对起止时间
		TArray<FAKScheduledTask> Cycle;
		Cycle.Reserve(Track->Tasks.Num());

		float Cursor = StartDelay;
		for (UAKAbilityTaskBase* Task : Track->Tasks)
		{
			if (!Task)
			{
				continue;
			}

			FAKScheduledTask Scheduled;
			Scheduled.Task = Task;
			Scheduled.TrackIndex = TrackIndex;

			if (Track->CompositeMode == EAKTrackCompositeMode::Parallel)
			{
				Scheduled.StartTime = StartDelay;
				Scheduled.EndTime = StartDelay + Task->Duration;
			}
			else if (Track->CompositeMode == EAKTrackCompositeMode::Sequence)
			{
				Scheduled.StartTime = Cursor;
				Scheduled.EndTime = Cursor + Task->Duration;
				Cursor = Scheduled.EndTime;
			}
			else // Timeline
			{
				Scheduled.StartTime = Task->StartTime + StartDelay;
				Scheduled.EndTime = Task->GetEndTime() + StartDelay;
			}

			Cycle.Add(Scheduled);
		}

		// 一轮的时长（用于重复间隔）
		float CycleLength = 0.f;
		for (const FAKScheduledTask& Scheduled : Cycle)
		{
			CycleLength = FMath::Max(CycleLength, Scheduled.EndTime - StartDelay);
		}

		for (int32 RepeatIndex = 0; RepeatIndex < Repeats; ++RepeatIndex)
		{
			const float Offset = RepeatIndex * (CycleLength + Interval);
			for (FAKScheduledTask Scheduled : Cycle)
			{
				Scheduled.StartTime += Offset;
				Scheduled.EndTime += Offset;
				OutPlan.Add(Scheduled);
			}
		}
	}

	OutPlan.Sort([](const FAKScheduledTask& A, const FAKScheduledTask& B)
	{
		if (A.StartTime != B.StartTime)
		{
			return A.StartTime < B.StartTime;
		}
		return A.TrackIndex < B.TrackIndex;
	});
}

void UAKAbilityAsset::Validate(FAKValidationResult& OutResult) const
{
	// 资产级校验
	if (Length <= 0.f)
	{
		OutResult.Add(EAKValidationSeverity::Error,
			LOCTEXT("InvalidLength", "技能长度必须大于 0。"), nullptr);
	}

	if (Tracks.Num() == 0)
	{
		OutResult.Add(EAKValidationSeverity::Warning,
			LOCTEXT("NoTracks", "该技能没有任何轨道，激活后不会做任何事。"), nullptr);
	}

	TArray<UAKAbilityTaskBase*> Tasks;
	CollectTasks(Tasks, false);
	if (Tasks.Num() == 0)
	{
		OutResult.Add(EAKValidationSeverity::Warning,
			LOCTEXT("NoTasks", "该技能没有任何任务。"), nullptr);
	}

	// 任务自带校验（内聚，而不是外部绑定 Validator）
	for (UAKAbilityTaskBase* Task : Tasks)
	{
		if (Task)
		{
			Task->Validate(this, OutResult);
		}
	}
}

#undef LOCTEXT_NAMESPACE
