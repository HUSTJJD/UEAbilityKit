#pragma once

#include "CoreMinimal.h"
#include "UAKChanneling.h"
#include "LyraAdaptInputCondition.generated.h"

/**
 * 「按住才维持」条件（Lyra 版）。
 *
 * UAK 有意不内置输入条件——输入绑定是项目相关的，钉死在框架里会让插件变脏
 * （见 UAKChanneling.h 里 UAKChanneling_CustomCondition 的说明）。
 * 所以 Lyra 这一侧自己实现一个：由 Lyra 的 Enhanced Input 绑定在
 * Pressed / Released 时调用 SetHeld()。
 *
 * 用法（在 Lyra 的输入绑定里）：
 *   Pressed  → Condition->SetHeld(true)
 *   Released → Condition->SetHeld(false)
 * 然后把它挂到 UAK 资产的 Channeling 条件里，松手即打断吟唱。
 */
UCLASS(BlueprintType)
class LYRAADAPT_API ULyraAdaptInputCondition : public UAKChanneling_CustomCondition
{
	GENERATED_BODY()

public:
	ULyraAdaptInputCondition();

	/** 由输入绑定调用 */
	UFUNCTION(BlueprintCallable, Category = "UAK|Lyra")
	void SetHeld(bool bInHeld);

	UFUNCTION(BlueprintCallable, Category = "UAK|Lyra")
	bool IsHeld() const { return bHeld; }

	// 基类 UAKChannelingCondition 的虚函数叫 IsSatisfied（不是 IsConditionSatisfied）
	virtual bool IsSatisfied(const FAKAbilityContext& Context) const override;

	/** 便于在输入绑定里定位：对应的 Lyra InputTag（如 InputTag.Weapon.Fire），仅作标记用 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UAK|Lyra", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;

private:
	bool bHeld = false;
};
