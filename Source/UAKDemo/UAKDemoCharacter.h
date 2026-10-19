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
 * 脚本只负责项目特定的扩展（自定义任务、自定义引导条件、UI 联动）。
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

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	UAbilitySystemComponent* AbilitySystemComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Abilities")
	UAKDemoAttributeSet* AttributeSet = nullptr;

	/** 初始授予的技能（填 UAKDemoAbility 的蓝图子类） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** 初始属性用的 GE（可选） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TSubclassOf<class UGameplayEffect> DefaultAttributesEffect;

private:
	void InitAbilityActorInfo();
	void GrantDefaultAbilities();
	bool bAbilityActorInfoInitialized = false;
};
