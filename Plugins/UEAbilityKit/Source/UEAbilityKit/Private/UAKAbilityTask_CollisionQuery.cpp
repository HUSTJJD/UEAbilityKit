#include "UAKAbilityTask_CollisionQuery.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityContext.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "UAKAbilityTask_CollisionQuery"

void UAKAbilityTask_CollisionQuery::OnTaskStart(FAKAbilityContext& Context)
{
	AActor* Owner = Context.Owner.Get();
	if (!Owner)
	{
		return;
	}

	UWorld* World = Owner->GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = Owner->GetActorLocation() + Owner->GetActorRotation().RotateVector(Offset);
	const FQuat Rotation = Owner->GetActorQuat();

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
	FCollisionQueryParams Params(SCENE_QUERY_STAT(UAKCollisionQuery), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(Owner);

	const bool bHit = World->OverlapMultiByChannel(Overlaps, Origin, Rotation, TraceChannel, CollisionShape, Params);
	if (!bHit)
	{
		if (!bAppendToTargets)
		{
			Context.Targets.Reset();
		}
		return;
	}

	if (!bAppendToTargets)
	{
		Context.Targets.Reset();
	}

	int32 Added = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor || HitActor == Owner)
		{
			continue;
		}

		if (bRequireAbilitySystem && !UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor))
		{
			continue;
		}

		Context.Targets.AddUnique(TWeakObjectPtr<AActor>(HitActor));
		++Added;

		if (MaxTargets > 0 && Added >= MaxTargets)
		{
			break;
		}
	}
}

void UAKAbilityTask_CollisionQuery::GetGASImpact(FAKGASImpact& OutImpact) const
{
	const FText ShapeName = StaticEnum<EAKQueryShape>()->GetDisplayValueAsText(Shape);
	OutImpact.Summary = FText::Format(
		LOCTEXT("QuerySummary", "查询 {0}：最多 {1} 个目标"),
		ShapeName, FText::AsNumber(MaxTargets));
}

void UAKAbilityTask_CollisionQuery::Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const
{
	Super::Validate(Asset, OutResult);

	if (Shape == EAKQueryShape::Sphere && Radius <= 0.f)
	{
		OutResult.Add(EAKValidationSeverity::Error,
			LOCTEXT("BadSphere", "球体查询的半径必须大于 0。"),
			const_cast<UAKAbilityTask_CollisionQuery*>(this));
	}

	// 查询之后如果没有任何效果任务使用目标，通常意味着漏配
	// （这里只做资产级提示，具体由资产校验统一汇总）
}

void UAKAbilityTask_CollisionQuery::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);

	OutJson->SetStringField(TEXT("shape"), StaticEnum<EAKQueryShape>()->GetNameStringByValue(static_cast<int64>(Shape)));
	OutJson->SetNumberField(TEXT("radius"), Radius);

	TArray<TSharedPtr<FJsonValue>> ExtentArray;
	ExtentArray.Add(MakeShared<FJsonValueNumber>(Extent.X));
	ExtentArray.Add(MakeShared<FJsonValueNumber>(Extent.Y));
	ExtentArray.Add(MakeShared<FJsonValueNumber>(Extent.Z));
	OutJson->SetArrayField(TEXT("extent"), ExtentArray);

	OutJson->SetNumberField(TEXT("halfHeight"), HalfHeight);
	OutJson->SetNumberField(TEXT("traceChannel"), static_cast<double>(TraceChannel.GetValue()));
	OutJson->SetBoolField(TEXT("appendToTargets"), bAppendToTargets);
	OutJson->SetNumberField(TEXT("maxTargets"), MaxTargets);
	OutJson->SetBoolField(TEXT("requireAbilitySystem"), bRequireAbilitySystem);
}

void UAKAbilityTask_CollisionQuery::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	FString ShapeName;
	if (InJson.TryGetStringField(TEXT("shape"), ShapeName))
	{
		if (const UEnum* Enum = StaticEnum<EAKQueryShape>())
		{
			Shape = static_cast<EAKQueryShape>(Enum->GetValueByNameString(ShapeName));
		}
	}

	double OutRadius = Radius;
	if (InJson.TryGetNumberField(TEXT("radius"), OutRadius))
	{
		Radius = static_cast<float>(OutRadius);
	}

	const TArray<TSharedPtr<FJsonValue>>* ExtentArray = nullptr;
	if (InJson.TryGetArrayField(TEXT("extent"), ExtentArray) && ExtentArray && ExtentArray->Num() == 3)
	{
		Extent.X = static_cast<float>((*ExtentArray)[0]->AsNumber());
		Extent.Y = static_cast<float>((*ExtentArray)[1]->AsNumber());
		Extent.Z = static_cast<float>((*ExtentArray)[2]->AsNumber());
	}

	double OutHalfHeight = HalfHeight;
	if (InJson.TryGetNumberField(TEXT("halfHeight"), OutHalfHeight))
	{
		HalfHeight = static_cast<float>(OutHalfHeight);
	}

	int32 ChannelValue = TraceChannel.GetValue();
	if (InJson.TryGetNumberField(TEXT("traceChannel"), OutHalfHeight))
	{
		ChannelValue = static_cast<int32>(OutHalfHeight);
	}
	TraceChannel = static_cast<ECollisionChannel>(ChannelValue);

	InJson.TryGetBoolField(TEXT("appendToTargets"), bAppendToTargets);

	double OutMaxTargets = MaxTargets;
	if (InJson.TryGetNumberField(TEXT("maxTargets"), OutMaxTargets))
	{
		MaxTargets = static_cast<int32>(OutMaxTargets);
	}

	InJson.TryGetBoolField(TEXT("requireAbilitySystem"), bRequireAbilitySystem);
}

#undef LOCTEXT_NAMESPACE
