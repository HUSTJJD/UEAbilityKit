#include "UAKAbilityTickTask.h"

#include "UAKAbilityInstance.h"

UAKAbilityTickTask::UAKAbilityTickTask()
{
	// 开启每帧 Tick（TickTask 才会被调用）
	bTickingTask = true;
}

void UAKAbilityTickTask::SetInstance(UAKAbilityInstance* InInstance)
{
	Instance = InInstance;
}

void UAKAbilityTickTask::TickTask(float DeltaTime)
{
	if (!Instance)
	{
		return;
	}

	if (!Instance->IsFinished())
	{
		Instance->Tick(DeltaTime);
	}

	// 时间轴跑完（或被 Finish）后主动结束任务，
	// 由 OnDestroy 通知 UAKGameplayAbility 结束技能。
	if (Instance->IsFinished() && !bEnding)
	{
		bEnding = true;
		EndTask();
	}
}

void UAKAbilityTickTask::OnDestroy(bool bInOwnerFinished)
{
	if (Instance && !Instance->IsFinished())
	{
		// 走到这里说明时间轴还没跑完就被外部结束了（技能结束 / 取消）→ 视为中断
		Instance->Finish(/*bInterrupted=*/true);
	}

	Instance = nullptr;

	// 文档要求：Super::OnDestroy 放到最后
	Super::OnDestroy(bInOwnerFinished);
}
