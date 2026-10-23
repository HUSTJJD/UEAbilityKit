#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UAKAbilityContext.h"
#include "UAKTypes.h"
#include "UAKAbilityInstance.generated.h"

class UAKAbilityAsset;
class UAKAbilityTaskBase;

/** 正在运行中的持续型任务 */
USTRUCT()
struct FAKActiveTask
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAKAbilityTaskBase> Task = nullptr;

	/** 该任务应当结束的时间（时间轴秒） */
	UPROPERTY()
	float EndTime = 0.f;

	UPROPERTY()
	bool bNeedsTick = false;

	UPROPERTY()
	bool bGate = false;

	/** 启动序号，用于中断时确定结束顺序 */
	UPROPERTY()
	int32 StartOrder = 0;

	UPROPERTY()
	int32 TrackIndex = INDEX_NONE;

	/** 对应的溯源节点索引（未开启记录时为 INDEX_NONE） */
	UPROPERTY()
	int32 TraceNodeIndex = INDEX_NONE;
};

DECLARE_DELEGATE_OneParam(FOnAKAbilityInstanceFinished, bool /*bInterrupted*/);

/**
 * 技能时间轴的一次运行实例。
 *
 * 职责：把「一条资产」推进成「一次施放」——时间推进、任务进入/结束、循环、中断、门控。
 * 不做的事：不碰网络（交给 GAS）、不做目标选择（交给 TargetData 与 Query 任务）、不做表现（交给 Cue）。
 *
 * 驱动方式：由 UAKAbilityTickTask（UAbilityTask 子类）每帧调用 Tick()。
 */
UCLASS(BlueprintType)
class UEABILITYKIT_API UAKAbilityInstance : public UObject
{
	GENERATED_BODY()

public:
	/** 用资产与上下文初始化，并展开执行计划（复合轨道 → 扁平调度表） */
	void Initialize(UAKAbilityAsset* InAsset, const FAKAbilityContext& InContext);

	/** 每帧推进（DeltaTime 为真实时间，内部按 PlayRate 缩放） */
	void Tick(float DeltaTime);

	/** 结束这次运行；bInterrupted 为 true 时所有存活任务按"被中断"结束 */
	void Finish(bool bInterrupted);

	bool IsFinished() const { return bFinished; }
	int32 GetLoopCount() const { return LoopCount; }
	bool IsGated() const;

	UAKAbilityAsset* GetAsset() const { return Asset; }

	// ── 实况查询（M4 联调 / M6 调试器用）─────────────────────────────────
	/** 当前时间轴时间（秒，已按 PlayRate 缩放） */
	float GetCurrentTime() const { return Context.CurrentTime; }

	/** 本次运行的时长（秒）：资产长度（循环时乘以循环次数由调用方决定） */
	float GetLength() const;

	/** 当前存活的任务列表（含结束时间、是否门控） */
	const TArray<FAKActiveTask>& GetActiveTasks() const { return ActiveTasks; }

	/** 本次运行的溯源记录（未开启记录时为 nullptr） */
	UAKTrace* GetTrace() const { return Trace; }
	FAKAbilityContext& GetContext() { return Context; }
	const FAKAbilityContext& GetContext() const { return Context; }

	/** 绑定溯源记录（传 nullptr 表示不记录，零开销） */
	void SetTrace(UAKTrace* InTrace);

	/** 时间轴跑完或被中断时触发（由 UAKGameplayAbility 监听并结束技能） */
	FOnAKAbilityInstanceFinished OnFinished;

private:
	void StartDueTasks();
	void TickActiveTasks(float ScaledDeltaTime);
	void EndDueTasks();

	/** 轨道被 Branch 停用后，取消该轨道上仍在运行的任务 */
	void EndTasksOnDisabledTracks();

	/** 中断时补执行"保证执行"但还没轮到的任务 */
	void RunGuaranteedPendingTasks();

	/** 引导：当前是否处于引导窗口内 */
	bool IsInChannelingWindow() const;

	/** 引导：本端是否需要判定引导失败 */
	bool ShouldEvaluateChanneling() const;

	/** 引导：所有维持条件是否仍成立 */
	bool AreChannelingConditionsSatisfied() const;

	/** 引导：按 ChannelFailureAction 处理失败 */
	void ApplyChannelingFailure();

	bool IsTrackDisabled(int32 TrackIndex) const;
	bool ShouldRunOnThisEnd(const UAKAbilityTaskBase* Task) const;

	/** 按资产的 InterruptOrder 排序，决定中断时的结束顺序 */
	void SortForInterrupt(TArray<FAKActiveTask>& InOut) const;

	void RestartLoop();

	UPROPERTY()
	TObjectPtr<UAKAbilityAsset> Asset = nullptr;

	/** 作为 UPROPERTY，使其中的 UObject 引用（ScratchPads）能被 GC 正确追踪 */
	UPROPERTY()
	FAKAbilityContext Context;

	/** 执行计划（按 StartTime 升序） */
	UPROPERTY()
	TArray<FAKScheduledTask> Plan;

	UPROPERTY()
	TArray<FAKActiveTask> ActiveTasks;

	UPROPERTY()
	TObjectPtr<UAKTrace> Trace = nullptr;

	int32 PendingIndex = 0;
	int32 LoopCount = 0;
	int32 RootNodeIndex = INDEX_NONE;
	int32 StartOrderCounter = 0;
	bool bFinished = false;
};
