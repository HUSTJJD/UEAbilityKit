#include "UAKAbilityTask_CollisionQuery.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityContext.h"

#include "UAKQuery.h"

#include "UAKTrace.h"
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

	// 锥形未指定轴向时用施法者正前方
	const FVector Direction = ConeDirection.IsNearlyZero()
		? Owner->GetActorForwardVector()
		: ConeDirection.GetSafeNormal();

	// 与 AreaEffect、投射物共用同一份查询实现
	TArray<AActor*> HitActors = UAKQuery::OverlapActors(
		World, Origin, Owner->GetActorQuat(), Shape, Radius, Extent, HalfHeight,
		TraceChannel, Owner, bRequireAbilitySystem, MaxTargets,
		Direction, ConeAngle, Filters);

	if (!bAppendToTargets)
	{
		Context.Targets.Reset();
	}

	for (AActor* HitActor : HitActors)
	{
		Context.Targets.AddUnique(TWeakObjectPtr<AActor>(HitActor));

		// 血缘下钻：把命中的目标记下来
		if (Context.Trace && Context.CurrentTaskNodeIndex != INDEX_NONE)
		{
			Context.Trace->AddNode(Context.CurrentTaskNodeIndex, EAKTraceNodeKind::Target,
				HitActor->GetName(), Context.CurrentTime);
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

	// 锥形参数
	OutJson->SetNumberField(TEXT("coneAngle"), ConeAngle);

	TArray<TSharedPtr<FJsonValue>> ConeDirArray;
	ConeDirArray.Add(MakeShared<FJsonValueNumber>(ConeDirection.X));
	ConeDirArray.Add(MakeShared<FJsonValueNumber>(ConeDirection.Y));
	ConeDirArray.Add(MakeShared<FJsonValueNumber>(ConeDirection.Z));
	OutJson->SetArrayField(TEXT("coneDirection"), ConeDirArray);

	// 过滤器
	TArray<TSharedPtr<FJsonValue>> FilterArray;
	UAKQueryFilter::ExportFilterArray(Filters, FilterArray);
	OutJson->SetArrayField(TEXT("filters"), FilterArray);

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

	// 锥形参数
	double OutConeAngle = ConeAngle;
	if (InJson.TryGetNumberField(TEXT("coneAngle"), OutConeAngle))
	{
		ConeAngle = static_cast<float>(OutConeAngle);
	}

	const TArray<TSharedPtr<FJsonValue>>* ConeDirArray = nullptr;
	if (InJson.TryGetArrayField(TEXT("coneDirection"), ConeDirArray) && ConeDirArray && ConeDirArray->Num() == 3)
	{
		ConeDirection.X = static_cast<float>((*ConeDirArray)[0]->AsNumber());
		ConeDirection.Y = static_cast<float>((*ConeDirArray)[1]->AsNumber());
		ConeDirection.Z = static_cast<float>((*ConeDirArray)[2]->AsNumber());
	}

	// 过滤器
	const TArray<TSharedPtr<FJsonValue>>* FilterArray = nullptr;
	if (InJson.TryGetArrayField(TEXT("filters"), FilterArray) && FilterArray)
	{
		UAKQueryFilter::ImportFilterArray(*FilterArray, GetOuter(), Filters);
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
