#pragma once

#include "CoreMinimal.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTask_ApplyGameplayEffect.generated.h"

class UGameplayEffect;

/**
 * 对目标施加 GameplayEffect。
 *
 * 这是"表现与逻辑分离、复用 GAS 原语"的范例任务：
 * 伤害、治疗、Buff、护盾、属性修改等一律建模为 GE，
 * 由 GAS 负责预测、复制与堆叠——UAK 不自建任何属性/修正器系统。
 */
UCLASS(meta = (DisplayName = "Apply Gameplay Effect"))
class UEABILITYKIT_API UAKAbilityTask_ApplyGameplayEffect : public UAKAbilityTaskBase
{
	GENERATED_BODY()

public:
	UAKAbilityTask_ApplyGameplayEffect();

	/** 要施加的 GE（伤害 / 治疗 / Buff / 护盾 / 属性修改…） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	TSubclassOf<UGameplayEffect> EffectClass;

	/** GE 等级（可用于 SetByCaller / 曲线取值） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Level = 1.f;

	/** SetByCaller 数值：让同一份资产在不同技能里复用不同数值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	TMap<FName, float> SetByCaller;

	/** 没有目标时是否施加到施法者自身 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	bool bApplyToSelfIfNoTarget = true;

	virtual void OnTaskStart(FAKAbilityContext& Context) override;
	virtual void GetGASImpact(FAKGASImpact& OutImpact) const override;
	virtual void Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const override;
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;
};
