#pragma once

#include "CoreMinimal.h"
#include "UAKCondition.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTask_Branch.generated.h"

/**
 * 条件分支：根据 GAS 的 Tag / 属性决定"另一条轨道是否参与本次运行"。
 *
 * 这是"时间轴只管何时、条件走规则"的落地方式：
 * - 不在时间轴里长条件树（Able 的 BranchTask 就是这么做的，会让时间轴难以阅读）；
 * - 也不照搬 Able 的 ECA 类结构；
 * - 而是用一条独立的 Rule 轨道 + GAS 原生条件去开关其它轨道。
 */
UCLASS(meta = (DisplayName = "Branch (轨道开关)"))
class UEABILITYKIT_API UAKAbilityTask_Branch : public UAKAbilityTaskBase
{
	GENERATED_BODY()

public:
	/** 判断条件（与 WaitUntil 共用 FAKCondition，语义一致） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FAKCondition Condition;

	// ── 动作 ──────────────────────────────────────────────────────────────
	/** 条件满足时要开关的轨道名 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	FName TargetTrackName = NAME_None;

	/** true = 启用目标轨道；false = 停用目标轨道 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	bool bEnableTrack = true;

	/** 条件不满足时是否执行相反动作 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	bool bApplyOppositeOnFail = false;

	virtual void OnTaskStart(FAKAbilityContext& Context) override;
	virtual void GetGASImpact(FAKGASImpact& OutImpact) const override;
	virtual void Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const override;
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;
};
