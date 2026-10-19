#include "UAKChanneling.h"

#include "UAKAbilityContext.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

#if WITH_EDITOR
FText UAKChannelingCondition::GetConditionDescription() const
{
	return FText::FromString(GetClass()->GetName().Replace(TEXT("UAKChanneling_"), TEXT("")));
}
#endif

bool UAKChanneling_TagCondition::IsSatisfied(const FAKAbilityContext& Context) const
{
	return Condition.Evaluate(Context);
}

bool UAKChanneling_MovementCondition::IsSatisfied(const FAKAbilityContext& Context) const
{
	const AActor* Owner = Context.Owner.Get();
	if (!Owner)
	{
		return true;
	}

	const FVector Velocity = Owner->GetVelocity();
	const float Speed = bUseXYSpeedOnly ? Velocity.Size2D() : Velocity.Size();

	// 条件成立 = 没有移动（速度在阈值内）
	return Speed <= SpeedThreshold;
}

bool UAKChanneling_DistanceCondition::IsSatisfied(const FAKAbilityContext& Context) const
{
	const AActor* Owner = Context.Owner.Get();
	if (!Owner)
	{
		return true;
	}

	AActor* Target = nullptr;
	for (const TWeakObjectPtr<AActor>& Candidate : Context.Targets)
	{
		if (Candidate.IsValid())
		{
			Target = Candidate.Get();
			break;
		}
	}

	if (!Target)
	{
		return bPassWhenNoTarget;
	}

	const FVector Delta = Target->GetActorLocation() - Owner->GetActorLocation();
	const float Distance = bUse2DDistance ? Delta.Size2D() : Delta.Size();

	float Threshold = DistanceThreshold;
	if (bUseCapsuleRadius)
	{
		auto RadiusOf = [](const AActor* Actor) -> float
		{
			if (const ACharacter* Character = Cast<ACharacter>(Actor))
			{
				return FMath::Max(Character->GetCapsuleComponent()->GetScaledCapsuleRadius(), 0.f);
			}
			return 0.f;
		};
		Threshold += RadiusOf(Target) + RadiusOf(Owner);
	}

	return Distance <= Threshold;
}

bool UAKChanneling_CustomCondition::IsSatisfiedBP_Implementation(const FAKAbilityContext& Context) const
{
	// 默认成立；由蓝图或 C++ 子类覆写
	return true;
}

bool UAKChanneling_CustomCondition::IsSatisfied(const FAKAbilityContext& Context) const
{
	return IsSatisfiedBP(Context);
}
