#pragma once

#include "CoreMinimal.h"
#include "UAKChanneling.h"
#include "UAKDemoConditions.generated.h"

class AAKDemoCharacter;

/**
 * 示例用的引导维持条件："这个技能槽的按键还按着吗"。
 *
 * UAK 刻意不内置输入系统——按键是项目相关的（Enhanced Input / 旧式映射 / 手柄 / UI 按钮）。
 * 但它留了一个类型安全的扩展点 UAKChannelingCondition：
 * 派生它并回答 IsSatisfied，就能把"输入松手即打断吟唱"这种项目特有语义接进来，
 * 框架本身不被任何一种输入方案绑死。
 *
 * 用法：在技能资产里把引导条件换成"按住输入槽位"，填上槽位号（对应 Lua 里的槽位）。
 */
UCLASS(meta = (DisplayName = "按住输入槽位"))
class UAKDEMO_API UAKDemoChanneling_InputHeld : public UAKChannelingCondition
{
	GENERATED_BODY()

public:
	/** 对应角色 DefaultAbilities 的下标（也对应 Lua 里的槽位号） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	int32 SlotIndex = 0;

	virtual bool IsSatisfied(const FAKAbilityContext& Context) const override;

	/** 输入类条件通常由客户端先感知，需要通知服务器取消 */
	virtual bool RequiresServerNotification() const override { return true; }

	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;

#if WITH_EDITOR
	virtual FText GetConditionDescription() const override;
#endif

private:
	AAKDemoCharacter* GetDemoCharacter(const FAKAbilityContext& Context) const;
};
