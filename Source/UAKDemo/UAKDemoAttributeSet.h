#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "UAKDemoAttributeSet.generated.h"

/**
 * 示例属性集：Health / MaxHealth / Mana / MaxMana。
 *
 * 这是纯 GAS 代码，UAK 不介入——属性系统归 GAS 管。
 * 注意属性必须复制（ReplicatedUsing + OnRep + GAMEPLAYATTRIBUTE_REPNOTIFY），
 * 否则客户端预测出的伤害会看起来"回滚"。
 */
UCLASS()
class UAKDEMO_API UAKDemoAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UAKDemoAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, Category = "Vital", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, Category = "Vital", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;

	UPROPERTY(BlueprintReadOnly, Category = "Vital", ReplicatedUsing = OnRep_Mana)
	FGameplayAttributeData Mana;

	UPROPERTY(BlueprintReadOnly, Category = "Vital", ReplicatedUsing = OnRep_MaxMana)
	FGameplayAttributeData MaxMana;

	ATTRIBUTE_ACCESSORS(UAKDemoAttributeSet, Health);
	ATTRIBUTE_ACCESSORS(UAKDemoAttributeSet, MaxHealth);
	ATTRIBUTE_ACCESSORS(UAKDemoAttributeSet, Mana);
	ATTRIBUTE_ACCESSORS(UAKDemoAttributeSet, MaxMana);

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth) const;

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const;

	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldMana) const;

	UFUNCTION()
	void OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana) const;
};
