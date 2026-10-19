#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "UAKDemoCharacter.generated.h"

class UAbilitySystemComponent;
class UAKDemoAttributeSet;
class UGameplayAbility;

/**
 * 示例角色：自带 ASC 与属性集。
 *
 * 简化起见 ASC 挂在 Character 上。真实项目里**玩家**建议挂 PlayerState
 * （死亡重生后属性与技能保留），AI 挂 Character 即可。
 * 详见 docs/examples/01-minimal-multiplayer-setup.md。
 */
UCLASS()
class UAKDEMO_API AAKDemoCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAKDemoCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }

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
