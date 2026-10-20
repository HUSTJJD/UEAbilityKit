#include "UAKChanneling.h"

#include "UAKAbilityContext.h"
#include "UAKTypes.h"

#include "Dom/JsonObject.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

void UAKChannelingCondition::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	OutJson->SetStringField(TEXT("$type"), GetClass()->GetName());
}

void UAKChannelingCondition::ImportJson(const FJsonObject& InJson)
{
	// 基类没有可恢复字段
}

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

void UAKChanneling_TagCondition::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);

	TSharedRef<FJsonObject> ConditionJson = MakeShared<FJsonObject>();
	Condition.ExportJson(ConditionJson);
	OutJson->SetObjectField(TEXT("condition"), ConditionJson);
}

void UAKChanneling_TagCondition::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	const TSharedPtr<FJsonObject>* ConditionJson = nullptr;
	if (InJson.TryGetObjectField(TEXT("condition"), ConditionJson) && ConditionJson && ConditionJson->IsValid())
	{
		Condition.ImportJson(**ConditionJson);
	}
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

void UAKChanneling_MovementCondition::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);
	OutJson->SetNumberField(TEXT("speedThreshold"), SpeedThreshold);
	OutJson->SetBoolField(TEXT("useXYSpeedOnly"), bUseXYSpeedOnly);
}

void UAKChanneling_MovementCondition::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	double OutThreshold = SpeedThreshold;
	if (InJson.TryGetNumberField(TEXT("speedThreshold"), OutThreshold))
	{
		SpeedThreshold = static_cast<float>(OutThreshold);
	}
	InJson.TryGetBoolField(TEXT("useXYSpeedOnly"), bUseXYSpeedOnly);
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

void UAKChanneling_DistanceCondition::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);
	OutJson->SetNumberField(TEXT("distanceThreshold"), DistanceThreshold);
	OutJson->SetBoolField(TEXT("use2DDistance"), bUse2DDistance);
	OutJson->SetBoolField(TEXT("useCapsuleRadius"), bUseCapsuleRadius);
	OutJson->SetBoolField(TEXT("passWhenNoTarget"), bPassWhenNoTarget);
}

void UAKChanneling_DistanceCondition::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	double OutThreshold = DistanceThreshold;
	if (InJson.TryGetNumberField(TEXT("distanceThreshold"), OutThreshold))
	{
		DistanceThreshold = static_cast<float>(OutThreshold);
	}
	InJson.TryGetBoolField(TEXT("use2DDistance"), bUse2DDistance);
	InJson.TryGetBoolField(TEXT("useCapsuleRadius"), bUseCapsuleRadius);
	InJson.TryGetBoolField(TEXT("passWhenNoTarget"), bPassWhenNoTarget);
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

void UAKChanneling_CustomCondition::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);
	OutJson->SetBoolField(TEXT("requiresServerNotification"), bRequiresServerNotification);
}

void UAKChanneling_CustomCondition::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);
	InJson.TryGetBoolField(TEXT("requiresServerNotification"), bRequiresServerNotification);
}
