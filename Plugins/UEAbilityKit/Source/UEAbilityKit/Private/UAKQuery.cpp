#include "UAKQuery.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UAKQueryFilter.h"
#include "UAKTypes.h"

TArray<AActor*> UAKQuery::OverlapActors(
	UWorld* World,
	const FVector& Origin,
	const FQuat& Rotation,
	EAKQueryShape Shape,
	float Radius,
	const FVector& Extent,
	float HalfHeight,
	TEnumAsByte<ECollisionChannel> TraceChannel,
	const AActor* IgnoreActor,
	bool bRequireAbilitySystem,
	int32 MaxResults,
	const FVector& ConeDirection,
	float ConeAngleDeg,
	const TArray<UAKQueryFilter*>& Filters)
{
	TArray<AActor*> Results;
	if (!World)
	{
		return Results;
	}

	FCollisionShape CollisionShape;
	switch (Shape)
	{
	case EAKQueryShape::Sphere:
		CollisionShape = FCollisionShape::MakeSphere(Radius);
		break;
	case EAKQueryShape::Box:
		CollisionShape = FCollisionShape::MakeBox(Extent);
		break;
	case EAKQueryShape::Capsule:
		CollisionShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
		break;
	case EAKQueryShape::Cone:
	default:
		// 锥形用球做广相，命中后再按夹角筛（窄相）
		CollisionShape = FCollisionShape::MakeSphere(Radius);
		break;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(UAKOverlap), /*bTraceComplex=*/false);
	if (IgnoreActor)
	{
		Params.AddIgnoredActor(IgnoreActor);
	}

	if (!World->OverlapMultiByChannel(Overlaps, Origin, Rotation, TraceChannel, CollisionShape, Params))
	{
		return Results;
	}

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor || HitActor == IgnoreActor)
		{
			continue;
		}

		if (bRequireAbilitySystem && !UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor))
		{
			continue;
		}

		// 锥形窄相：广相用球，这里再按与轴向的夹角筛掉
		if (Shape == EAKQueryShape::Cone && ConeAngleDeg > 0.f)
		{
			const FVector Direction = ConeDirection.GetSafeNormal();
			const FVector ToActor = (HitActor->GetActorLocation() - Origin).GetSafeNormal();
			const float CosAngle = FMath::Clamp(FVector::DotProduct(ToActor, Direction), -1.f, 1.f);
			const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(CosAngle));

			if (AngleDeg > ConeAngleDeg)
			{
				continue;
			}
		}

		// 过滤器链（Source 传的是施法者，即 IgnoreActor）
		bool bPassedFilters = true;
		for (const UAKQueryFilter* Filter : Filters)
		{
			if (Filter && !Filter->Passes(HitActor, IgnoreActor))
			{
				bPassedFilters = false;
				break;
			}
		}

		if (!bPassedFilters)
		{
			continue;
		}

		Results.AddUnique(HitActor);

		if (MaxResults > 0 && Results.Num() >= MaxResults)
		{
			break;
		}
	}

	return Results;
}

bool UAKQuery::SweepTo(
	UWorld* World,
	const FVector& Start,
	const FVector& End,
	float Radius,
	TEnumAsByte<ECollisionChannel> TraceChannel,
	const AActor* IgnoreActor,
	FHitResult& OutHit)
{
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(UAKProjectileSweep), /*bTraceComplex=*/false);
	if (IgnoreActor)
	{
		Params.AddIgnoredActor(IgnoreActor);
	}

	const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	return World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, TraceChannel, Shape, Params);
}
