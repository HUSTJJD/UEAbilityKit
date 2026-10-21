#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "UnLuaNovaInterface.h"
#include "GameFramework/Character.h"
#include "UAKDemoCharacter.generated.h"

class UAbilitySystemComponent;
class UAKDemoAttributeSet;
class UGameplayAbility;

/**
 * 示例角色：自带 ASC 与属性集，并实现 UnLuaNova 接口把逻辑交给 Lua。
 *
 * 简化起见 ASC 挂在 Character 上。真实项目里**玩家**建议挂 PlayerState
 * （死亡重生后属性与技能保留），AI 挂 Character 即可。
 * 详见 docs/examples/01-minimal-multiplayer-setup.md。
 *
 * 关于脚本：UAK 的核心（执行器、任务、编辑器）是纯 C++，不依赖脚本；
 * 脚本只负责项目特定的扩展（输入按下之后干什么、UI 联动、自定义引导条件）。
 * 见 docs/examples/04-scripting-with-unlua.md。
 */
UCLASS()
class UAKDEMO_API AAKDemoCharacter : public ACharacter, public IAbilitySystemInterface, public IUnLuaNovaInterface
{
	GENERATED_BODY()

public:
	AAKDemoCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }

	/** UnLuaNova：返回绑定的 Lua 模块名（对应 Content/Script/UAKDemoCharacter.lua） */
	virtual FString GetModuleName_Implementation() const override;

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	/** 供 UI 读取属性 */
	const UAKDemoAttributeSet* GetSampleAttributeSet() const { return AttributeSet; }

	// ── 技能触发 ──────────────────────────────────────────────────────
	/** 按 DefaultAbilities 的下标激活技能（走 GAS 的冷却 / 消耗 / 预测） */
	UFUNCTION(BlueprintCallable, Category = "UAKDemo|Abilities")
	bool TryActivateAbilityByIndex(int32 Index);

	/**
	 * 输入事件（由 C++ 绑定、由脚本实现）。
	 *
	 * 为什么绑输入不用 Enhanced Input：它需要 InputAction / MappingContext 资产，
	 * 一加就破坏"clone 下来直接能跑"。示例用旧式动作映射（见 Config/DefaultInput.ini），
	 * 真实项目把 SetupPlayerInputComponent 换成 BindAction 即可，Lua 侧不用改。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UAKDemo|Input")
	void K2_OnAbilityInput(int32 SlotIndex, bool bPressed);

	/** 技能开始 / 结束的通知（UI 读条、音效联动用） */
	UFUNCTION(BlueprintImplementableEvent, Category = "UAKDemo|Abilities")
	void K2_OnAbilityStarted(FName AbilityName);

	UFUNCTION(BlueprintImplementableEvent, Category = "UAKDemo|Abilities")
	void K2_OnAbilityEnded(FName AbilityName, bool bWasCancelled);

	// ── 引导维持条件（项目特有，UAK 不内置输入系统）─────────────────────
	/** 脚本在收到 K2_OnAbilityInput 时调用它，告诉 C++ 某个槽位是否还按着 */
	UFUNCTION(BlueprintCallable, Category = "UAKDemo|Input")
	void SetChannelingInputHeld(int32 SlotIndex, bool bHeld);

	/** 默认 true（没有脚本时不阻断引导）；脚本显式"松开"后才算不成立 */
	bool IsChannelingInputHeld(int32 SlotIndex) const;

protected:
	// AActor
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	UAbilitySystemComponent* AbilitySystemComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Abilities")
	UAKDemoAttributeSet* AttributeSet = nullptr;

	/** 初始授予的技能（填 UAKDemoAbility 的蓝图子类） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** 初始属性用的 GE（可选，示例里是 UAKDemoGE_DefaultAttributes） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TSubclassOf<class UGameplayEffect> DefaultAttributesEffect;

private:
	void InitAbilityActorInfo();
	void GrantDefaultAbilities();

	/** 输入绑定的落点：转发给脚本实现 */
	void NotifyAbilityInput(int32 SlotIndex, bool bPressed);

	void HandleAbility1Pressed();
	void HandleAbility1Released();
	void HandleAbility2Pressed();
	void HandleAbility2Released();

	bool bAbilityActorInfoInitialized = false;

	/** 被显式标记为"已松开"的槽位（默认都算按着，见 IsChannelingInputHeld） */
	TSet<int32> ReleasedInputSlots;
};
