#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UAKTypes.h"
#include "UAKQuery.generated.h"

class AActor;
class UWorld;

/**
 * 空间查询工具：被 CollisionQuery 任务、AreaEffect 任务与投射物共用。
 *
 * 只做"查询"，不做"命中处理"——命中后发生什么由 GE / Cue 任务决定，
 * 这样同一份查询能被伤害、治疗、上 Buff 等多种用途复用（AbilityKit 的原子能力思路）。
 */
UCLASS()
class UEABILITYKIT_API UAKQuery : public UObject
{
	GENERATED_BODY()

public:
	/** 形状重叠查询，返回命中的 Actor（已按形状、锥形角度与过滤器筛过） */
	static TArray<AActor*> OverlapActors(
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
		const FVector& ConeDirection = FVector::ForwardVector,
		float ConeAngleDeg = 0.f,
		const TArray<class UAKQueryFilter*>& Filters = TArray<class UAKQueryFilter*>());

	/** 射线/球体扫掠，用于投射物逐帧推进时的命中检测 */
	static bool SweepTo(
		UWorld* World,
		const FVector& Start,
		const FVector& End,
		float Radius,
		TEnumAsByte<ECollisionChannel> TraceChannel,
		const AActor* IgnoreActor,
		struct FHitResult& OutHit);
};
