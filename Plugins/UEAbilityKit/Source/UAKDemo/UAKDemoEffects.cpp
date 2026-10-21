#include "UAKDemoEffects.h"

#include "UAKDemoAttributeSet.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Demo_Cooldown, "Cooldown.Demo.Sample");

namespace
{
	/** 拼一条 Modifier：属性 + 运算 + 标量数值 */
	FGameplayModifierInfo MakeModifier(
		const FGameplayAttribute& Attribute,
		EGameplayModOp::Type Op,
		float Magnitude)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = Op;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Magnitude));
		return Modifier;
	}

	/** 拼一条"数值由调用方给"的 Modifier（SetByCaller，按 DataName 匹配） */
	FGameplayModifierInfo MakeSetByCallerModifier(
		const FGameplayAttribute& Attribute,
		EGameplayModOp::Type Op,
		const FName& DataName)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = Op;

		FSetByCallerFloat ByCaller;
		ByCaller.DataName = DataName;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(ByCaller);

		return Modifier;
	}
}

UAKDemoGE_MeleeDamage::UAKDemoGE_MeleeDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// 伤害数值由调用方（技能任务的 SetByCaller）给：
	// 这样"这 120 伤害是哪来的"在血缘树里能一路下钻到具体的数值，
	// 而不是只看到一个写死在 GE 里的常量。
	Modifiers.Add(MakeSetByCallerModifier(
		UAKDemoAttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, FName(TEXT("Damage"))));
}

UAKDemoGE_FireballDamage::UAKDemoGE_FireballDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeSetByCallerModifier(
		UAKDemoAttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, FName(TEXT("Damage"))));
}

UAKDemoGE_Cooldown::UAKDemoGE_Cooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.f));

	// GAS 的冷却靠"授予 Tag + 该 Tag 在技能的 BlockedTags 里"实现，UAK 不自建冷却
	FInheritedTagContainer Tags;
	Tags.AddTag(TAG_Demo_Cooldown);
	InheritableOwnedTagsContainer = Tags;
}

UAKDemoGE_Cost::UAKDemoGE_Cost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeModifier(UAKDemoAttributeSet::GetManaAttribute(), EGameplayModOp::Additive, -20.f));
}

UAKDemoGE_DefaultAttributes::UAKDemoGE_DefaultAttributes()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeModifier(UAKDemoAttributeSet::GetMaxHealthAttribute(), EGameplayModOp::Override, 100.f));
	Modifiers.Add(MakeModifier(UAKDemoAttributeSet::GetHealthAttribute(), EGameplayModOp::Override, 100.f));
	Modifiers.Add(MakeModifier(UAKDemoAttributeSet::GetMaxManaAttribute(), EGameplayModOp::Override, 50.f));
	Modifiers.Add(MakeModifier(UAKDemoAttributeSet::GetManaAttribute(), EGameplayModOp::Override, 50.f));
}
