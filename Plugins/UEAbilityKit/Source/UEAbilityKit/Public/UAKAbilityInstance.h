#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UAKAbilityContext.h"
#include "UAKAbilityInstance.generated.h"

class UAKAbilityAsset;
class UAKAbilityTaskBase;
class UAKTrace;

/** 正在运行中的持续型任务 */
USTRUCT()
struct FAKActiveTask
{
	GENERATED_BODY()

	UPROPERTY()
	UAKAbilityTaskBase* Task = nullptr;

	/** 该任务应当结束的时间（时间轴秒） */
	UPROPERTY()
	float EndTime = 0.f;

	UPROPERTY()
	bool bNeedsTick = false;

	/** 对应的溯源节点索引（未开启记录时为 INDEX_NONE） */
	UPROPERTY()
	int32 TraceNodeIndex = INDEX_NONE;
};

DECLARE_DELEGATE_OneParam(FOnAKAbilityInstanceFinished, bool /*bInterrupted*/);

/**
 * 技能时间轴的一次运行实例（对标 Able 的 UAblAbilityInstance，但语义更严格）。
 *
 * 职责：把「一条资产」推进成「一次施放」——负责时间推进、任务进入/结束、循环与中断。
 * 不做的事：不碰网络（交给 GAS）、不做目标选择（交给 Task 与 TargetData）、不做表现（交给 Cue）。
 *
 * 驱动方式：由 UAKAbilityTickTask（UAbilityTask 子类）每帧调用 Tick()。
 * 这样时间轴随技能生命周期自动结束，且天然处于 GAS 的能力作用域内。
 */
UCLASS(BlueprintType)
class UEABILITYKIT_API UAKAbilityInstance : public UObject
{
	GENERATED_BODY()

public:
	/** 用资产与上下文初始化，并把待执行任务按 StartTime 排序 */
	void Initialize(UAKAbilityAsset* InAsset, const FAKAbilityContext& InContext);

	/** 每帧推进（DeltaTime 为真实时间，内部按 PlayRate 缩放） */
	void Tick(float DeltaTime);

	/** 结束这次运行；bInterrupted 为 true 时所有存活任务按"被中断"结束 */
	void Finish(bool bInterrupted);

	bool IsFinished() const { return bFinished; }
	int32 GetLoopCount() const { return LoopCount; }

	/** 绑定溯源记录（传 nullptr 表示不记录，零开销） */
	void SetTrace(UAKTrace* InTrace);

	UAKAbilityAsset* GetAsset() const { return Asset; }
	FAKAbilityContext& GetContext() { return Context; }
	const FAKAbilityContext& GetContext() const { return Context; }

	/** 时间轴跑完或被中断时触发（由 UAKGameplayAbility 监听并结束技能） */
	FOnAKAbilityInstanceFinished OnFinished;

private:
	void StartDueTasks();
	void TickActiveTasks(float ScaledDeltaTime);
	void EndDueTasks();
	void RestartLoop();

	UPROPERTY()
	UAKAbilityAsset* Asset = nullptr;

	/** 作为 UPROPERTY，使其中的 UObject 引用（ScratchPads）能被 GC 正确追踪 */
	UPROPERTY()
	FAKAbilityContext Context;

	/** 按 StartTime 升序排列的待执行任务 */
	UPROPERTY()
	TArray<UAKAbilityTaskBase*> PendingTasks;

	UPROPERTY()
	TArray<FAKActiveTask> ActiveTasks;

	UPROPERTY()
	UAKTrace* Trace = nullptr;

	int32 PendingIndex = 0;
	int32 LoopCount = 0;
	int32 RootNodeIndex = INDEX_NONE;
	bool bFinished = false;
};
