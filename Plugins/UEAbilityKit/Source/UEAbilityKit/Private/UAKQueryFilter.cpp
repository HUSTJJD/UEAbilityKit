#include "UAKQueryFilter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

#if WITH_EDITOR
FText UAKQueryFilter::GetFilterDescription() const
{
	return FText::FromString(GetClass()->GetName().Replace(TEXT("UAKQueryFilter_"), TEXT("")));
}
#endif

bool UAKQueryFilter_Tag::Passes(const AActor* Candidate, const AActor* Source) const
{
	if (!Candidate)
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate);
	if (!TargetASC)
	{
		return bKeepIfNoAbilitySystem;
	}

	if (RequiredTags.Num() > 0 && !TargetASC->HasAllMatchingGameplayTags(RequiredTags))
	{
		return false;
	}

	if (BlockedTags.Num() > 0 && TargetASC->HasAnyMatchingGameplayTags(BlockedTags))
	{
		return false;
	}

	return true;
}

bool UAKQueryFilter_Distance::Passes(const AActor* Candidate, const AActor* Source) const
{
	if (!Candidate || !Source)
	{
		return false;
	}

	const FVector Delta = Candidate->GetActorLocation() - Source->GetActorLocation();
	const float Distance = bUse2DDistance ? Delta.Size2D() : Delta.Size();

	float Threshold = MaxDistance;
	if (bUseCapsuleRadius)
	{
		// 粗略计入双方胶囊半径
		auto RadiusOf = [](const AActor* Actor) -> float
		{
			if (const ACharacter* Character = Cast<ACharacter>(Actor))
			{
				return FMath::Max(Character->GetCapsuleComponent()->GetScaledCapsuleRadius(), 0.f);
			}
			return 0.f;
		};
		Threshold += RadiusOf(Candidate) + RadiusOf(Source);
	}

	return Distance <= Threshold;
}

bool UAKQueryFilter_LineOfSight::Passes(const AActor* Candidate, const AActor* Source) const
{
	if (!Candidate || !Source)
	{
		return false;
	}

	UWorld* World = Source->GetWorld();
	if (!World)
	{
		return true; // 没有世界信息时不拦
	}

	FVector Start = Source->GetActorLocation();
	FVector End = Candidate->GetActorLocation();

	if (bTraceFromEyeHeight)
	{
		if (const ACharacter* SourceCharacter = Cast<ACharacter>(Source))
		{
			Start.Z += SourceCharacter->BaseEyeHeight;
		}
		if (const ACharacter* TargetCharacter = Cast<ACharacter>(Candidate))
		{
			End.Z += TargetCharacter->BaseEyeHeight * 0.5f;
		}
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(UAKLineOfSight), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(Source);
	Params.AddIgnoredActor(Candidate);

	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params);

	// 命中了别的东西 = 被遮挡
	return !bBlocked;
}
