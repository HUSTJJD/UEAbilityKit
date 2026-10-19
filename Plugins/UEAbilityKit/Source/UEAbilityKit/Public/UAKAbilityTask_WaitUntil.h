#pragma once

#include "CoreMinimal.h"
#include "UAKCondition.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTask_WaitUntil.generated.h"

/**
 * 等待：条件满足即继续，或超时后继续。
 *
 * 勾选 bGateTimeline 时，该任务存活期间**整个时间轴的时间不前进**
 * （持续任务仍会收到 Tick），用于"等目标进入范围 / 等玩家松手"这类语义。
 *
 * 这是 AbilityKit 的 WaitUntil / Gate 思想在 UAK 的落地。
 */
UCLASS(meta = (DisplayName = "Wait Until"))
class UEABILITYKIT_API UAKAbilityTask_WaitUntil : public UAKAbilityTaskBase
{
	GENERATED_BODY()

public:
	UAKAbilityTask_WaitUntil();

	/** 等待的条件（与 Branch 共用 FAKCondition） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wait")
	FAKCondition Condition;

	/** 超时时间（秒）；到时无论条件是否满足都继续 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wait", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Timeout = 1.f;

	/** 超时时是否视为"失败"（仅用于溯源记录，不改变流程） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wait")
	bool bTreatTimeoutAsFailure = false;

	virtual void OnTaskStart(FAKAbilityContext& Context) override;
	virtual bool ShouldEndEarly(const FAKAbilityContext& Context) const override;
	virtual void OnTaskEnd(FAKAbilityContext& Context, bool bInterrupted) override;
	virtual void GetGASImpact(FAKGASImpact& OutImpact) const override;
	virtual void Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const override;
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;
};
