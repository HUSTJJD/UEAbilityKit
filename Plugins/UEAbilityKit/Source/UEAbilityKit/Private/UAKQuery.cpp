#include "UAKQuery.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

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
	int32 MaxResults)
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
	default:
		CollisionShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
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
