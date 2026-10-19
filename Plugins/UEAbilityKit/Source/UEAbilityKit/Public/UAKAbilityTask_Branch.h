#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTask_Branch.generated.h"

/** 数值比较方式 */
UENUM(BlueprintType)
enum class EAKComparison : uint8
{
	GreaterOrEqual	UMETA(DisplayName = "≥"),
	Greater			UMETA(DisplayName = ">"),
	LessOrEqual		UMETA(DisplayName = "≤"),
	Less			UMETA(DisplayName = "<"),
	Equal			UMETA(DisplayName = "==")
};

/**
 * 条件分支：根据 GAS 的 Tag / 属性决定"另一条轨道是否参与本次运行"。
 *
 * 这是"时间轴只管何时、条件走规则"的落地方式：
 * - 不在时间轴里长条件树（Able 的 BranchTask 就是这么做的，会让时间轴难以阅读）；
 * - 也不照搬 Able 的 ECA 类结构；
 * - 而是用一条独立的 Rule 轨道 + GAS 原生条件（Tag / 属性）去开关其它轨道。
 */
UCLASS(meta = (DisplayName = "Branch (轨道开关)"))
class UEABILITYKIT_API UAKAbilityTask_Branch : public UAKAbilityTaskBase
{
	GENERATED_BODY()

public:
	/** 需要全部满足的 Tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FGameplayTagContainer RequiredTags;

	/** 存在任一即不满足的 Tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FGameplayTagContainer BlockedTags;

	/** 参与比较的属性（留空表示只判断 Tag） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	EAKComparison Comparison = EAKComparison::GreaterOrEqual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	float Threshold = 0.f;

	/** 条件对象：默认取施法者，勾选后取第一个目标 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	bool bEvaluateOnFirstTarget = false;

	/** 取反 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	bool bInvert = false;

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
	virtual bool EvaluateCondition(const FAKAbilityContext& Context) const;
	virtual void GetGASImpact(FAKGASImpact& OutImpact) const override;
	virtual void Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const override;
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;
};
