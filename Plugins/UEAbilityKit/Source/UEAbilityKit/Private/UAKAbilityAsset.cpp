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
