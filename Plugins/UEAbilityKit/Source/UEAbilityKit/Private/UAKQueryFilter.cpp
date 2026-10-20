#include "UAKQueryFilter.h"

#include "Dom/JsonObject.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

void UAKQueryFilter::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	OutJson->SetStringField(TEXT("$type"), GetClass()->GetName());
}

void UAKQueryFilter::ImportJson(const FJsonObject& InJson)
{
	// 基类没有可恢复字段
}

void UAKQueryFilter::ExportFilterArray(const TArray<UAKQueryFilter*>& Filters, TArray<TSharedPtr<FJsonValue>>& OutArray)
{
	OutArray.Reset();
	for (const UAKQueryFilter* Filter : Filters)
	{
		if (!Filter)
		{
			continue;
		}
		TSharedRef<FJsonObject> FilterJson = MakeShared<FJsonObject>();
		Filter->ExportJson(FilterJson);
		OutArray.Add(MakeShared<FJsonValueObject>(FilterJson));
	}
}

void UAKQueryFilter::ImportFilterArray(const TArray<TSharedPtr<FJsonValue>>& InArray, UObject* Outer, TArray<UAKQueryFilter*>& OutFilters)
{
	OutFilters.Reset();
	for (const TSharedPtr<FJsonValue>& Value : InArray)
	{
		const TSharedPtr<FJsonObject>* FilterObject = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(FilterObject) || !FilterObject || !FilterObject->IsValid())
		{
			continue;
		}

		FString TypeName;
		(*FilterObject)->TryGetStringField(TEXT("$type"), TypeName);
		if (TypeName.IsEmpty())
		{
			continue;
		}

		UClass* FilterClass = LoadClass<UAKQueryFilter>(nullptr, *FString::Printf(TEXT("/Script/UEAbilityKit.%s"), *TypeName));
		if (!FilterClass)
		{
			FilterClass = LoadClass<UAKQueryFilter>(nullptr, *TypeName); // 蓝图派生
		}
		if (!FilterClass || FilterClass->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		UAKQueryFilter* Filter = NewObject<UAKQueryFilter>(Outer, FilterClass);
		if (!Filter)
		{
			continue;
		}

		Filter->ImportJson(**FilterObject);
		OutFilters.Add(Filter);
	}
}

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

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Candidate));
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

void UAKQueryFilter_Tag::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);
	OutJson->SetStringField(TEXT("requiredTags"), RequiredTags.ToString());
	OutJson->SetStringField(TEXT("blockedTags"), BlockedTags.ToString());
	OutJson->SetBoolField(TEXT("keepIfNoAbilitySystem"), bKeepIfNoAbilitySystem);
}

void UAKQueryFilter_Tag::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	auto ImportTags = [&InJson](const FString& Field, FGameplayTagContainer& OutTags)
	{
		FString TagsString;
		if (InJson.TryGetStringField(Field, TagsString) && !TagsString.IsEmpty())
		{
			OutTags.Reset();
			OutTags.FromExportString(TagsString);
		}
	};
	ImportTags(TEXT("requiredTags"), RequiredTags);
	ImportTags(TEXT("blockedTags"), BlockedTags);
	InJson.TryGetBoolField(TEXT("keepIfNoAbilitySystem"), bKeepIfNoAbilitySystem);
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

void UAKQueryFilter_Distance::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);
	OutJson->SetNumberField(TEXT("maxDistance"), MaxDistance);
	OutJson->SetBoolField(TEXT("use2DDistance"), bUse2DDistance);
	OutJson->SetBoolField(TEXT("useCapsuleRadius"), bUseCapsuleRadius);
}

void UAKQueryFilter_Distance::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	double OutDistance = MaxDistance;
	if (InJson.TryGetNumberField(TEXT("maxDistance"), OutDistance))
	{
		MaxDistance = static_cast<float>(OutDistance);
	}
	InJson.TryGetBoolField(TEXT("use2DDistance"), bUse2DDistance);
	InJson.TryGetBoolField(TEXT("useCapsuleRadius"), bUseCapsuleRadius);
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

void UAKQueryFilter_LineOfSight::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);
	OutJson->SetNumberField(TEXT("traceChannel"), static_cast<double>(TraceChannel.GetValue()));
	OutJson->SetBoolField(TEXT("traceFromEyeHeight"), bTraceFromEyeHeight);
}

void UAKQueryFilter_LineOfSight::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	double OutChannel = TraceChannel.GetValue();
	if (InJson.TryGetNumberField(TEXT("traceChannel"), OutChannel))
	{
		TraceChannel = static_cast<ECollisionChannel>(static_cast<int32>(OutChannel));
	}
	InJson.TryGetBoolField(TEXT("traceFromEyeHeight"), bTraceFromEyeHeight);
}
