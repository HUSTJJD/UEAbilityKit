#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "UAKAbilityTickTask.generated.h"

class UAKAbilityInstance;

/**
 * 时间轴的驱动任务。
 *
 * 为什么用 UAbilityTask 驱动（而不是 Ticker / 组件）：
 * - 生命周期与技能严格绑定：技能结束 → 任务自动结束 → 时间轴自动收尾；
 * - 位于 GAS 的能力作用域内，不引入额外的全局 Tick 与生命周期管理；
 * - UAK 逻辑层依旧不碰网络，网络的预测与复制仍由 GAS 负责。
 */
UCLASS()
class UEABILITYKIT_API UAKAbilityTickTask : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAKAbilityTickTask();

	void SetInstance(UAKAbilityInstance* InInstance);

	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	UPROPERTY()
	TObjectPtr<UAKAbilityInstance> Instance = nullptr;

	/** 防止重复调用 EndTask */
	bool bEnding = false;
};
