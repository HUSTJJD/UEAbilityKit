#include "UAKAbilityTask_AreaEffect.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityContext.h"
#include "UAKQuery.h"
#include "UAKTypes.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"

#define LOCTEXT_NAMESPACE "UAKAbilityTask_AreaEffect"

UAKAbilityTask_AreaEffect::UAKAbilityTask_AreaEffect()
{
	bNeedsTick = true;
	// 逻辑效果：交给权威端
	NetMode = EAKTaskNetMode::ServerOnly;
}

UObject* UAKAbilityTask_AreaEffect::CreateScratchPad(const FAKAbilityContext& Context)
{
	// 三元运算符两侧类型必须一致（AActor* / UPackage*），先统一成 UObject*
	UObject* Outer = Context.Owner.Get()
		? static_cast<UObject*>(Context.Owner.Get())
		: static_cast<UObject*>(GetTransientPackage());

	return NewObject<UAKAreaEffectScratchPad>(Outer);
}

FVector UAKAbilityTask_AreaEffect::GetCenter(const FAKAbilityContext& Context) const
{
	AActor* Owner = Context.Owner.Get();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}

	const FVector Center = Owner->GetActorLocation() + Owner->GetActorRotation().RotateVector(Offset);

	// 不跟随施法者：把首次生效时的中心固定下来
	if (!bFollowOwner)
	{
		if (UAKAreaEffectScratchPad* Pad = Cast<UAKAreaEffectScratchPad>(Context.ScratchPads.FindRef(this)))
		{
			if (!Pad->bCenterCaptured)
			{
				Pad->FixedCenter = Center;
				Pad->bCenterCaptured = true;
				return Center;
			}
			return Pad->FixedCenter;
		}
	}

	return Center;
}

void UAKAbilityTask_AreaEffect::ApplyOnce(FAKAbilityContext& Context)
{
	AActor* Owner = Context.Owner.Get();
	UAbilitySystemComponent* SourceASC = Context.SourceASC.Get();
	if (!Owner || !SourceASC || !EffectClass)
	{
		return;
	}

	UAKAreaEffectScratchPad* Pad = Cast<UAKAreaEffectScratchPad>(Context.ScratchPads.FindRef(this));
	if (!Pad)
	{
		return;
	}

	const FVector Center = GetCenter(Context);

	TArray<AActor*> HitActors = UAKQuery::OverlapActors(
		Owner->GetWorld(), Center, FQuat::Identity, EAKQueryShape::Sphere, Radius,
		FVector::ZeroVector, 0.f, ECC_Pawn, Owner, bRequireAbilitySystem, 0,
		FVector::ForwardVector, 0.f, Filters);

	int32 Applied = 0;
	for (AActor* HitActor : HitActors)
	{
		if (!HitActor)
		{
			continue;
		}

		if (bApplyOncePerTarget && Pad->HitActors.Contains(HitActor))
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		if (!TargetASC)
		{
			continue;
		}

		const FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
		const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(EffectClass, EffectLevel, EffectContext);
		if (SpecHandle.IsValid())
		{
			SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
		}

		if (bApplyOncePerTarget)
		{
			Pad->HitActors.Add(HitActor);
		}

		++Applied;
		if (MaxTargetsPerTick > 0 && Applied >= MaxTargetsPerTick)
		{
			break;
		}
	}

	if (CueTag.IsValid())
	{
		SourceASC->ExecuteGameplayCue(CueTag);
	}
}

void UAKAbilityTask_AreaEffect::OnTaskStart(FAKAbilityContext& Context)
{
	// 进入即生效一次，随后按 Interval 周期生效
	ApplyOnce(Context);
}

void UAKAbilityTask_AreaEffect::OnTaskTick(FAKAbilityContext& Context, float DeltaTime)
{
	UAKAreaEffectScratchPad* Pad = Cast<UAKAreaEffectScratchPad>(Context.ScratchPads.FindRef(this));
	if (!Pad)
	{
		return;
	}

	Pad->Timer += DeltaTime;
	if (Pad->Timer < Interval)
	{
		return;
	}

	Pad->Timer -= Interval;
	ApplyOnce(Context);
}

void UAKAbilityTask_AreaEffect::GetGASImpact(FAKGASImpact& OutImpact) const
{
	if (EffectClass)
	{
		OutImpact.Effects.AddUnique(TSoftClassPtr<UGameplayEffect>(EffectClass));
	}
	if (CueTag.IsValid())
	{
		OutImpact.Cues.AddUnique(CueTag);
	}

	OutImpact.Summary = FText::Format(
		LOCTEXT("AreaSummary", "半径 {0} · 每 {1}s 生效一次"),
		FText::AsNumber(Radius), FText::AsNumber(Interval));
}

void UAKAbilityTask_AreaEffect::Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const
{
	Super::Validate(Asset, OutResult);

	if (!EffectClass)
	{
		OutResult.Add(EAKValidationSeverity::Error,
			LOCTEXT("NoAreaEffectClass", "AreaEffect 未指定 GE。"),
			const_cast<UAKAbilityTask_AreaEffect*>(this));
	}

	if (Radius <= 0.f)
	{
		OutResult.Add(EAKValidationSeverity::Error,
			LOCTEXT("BadAreaRadius", "AreaEffect 的半径必须大于 0。"),
			const_cast<UAKAbilityTask_AreaEffect*>(this));
	}

	// 周期生效需要"持续型"任务：Duration 必须大于 0
	if (IsSingleFrame())
	{
		OutResult.Add(EAKValidationSeverity::Warning,
			LOCTEXT("AreaSingleFrame", "AreaEffect 的持续时间为 0，只会生效一次。"),
			const_cast<UAKAbilityTask_AreaEffect*>(this));
	}

	if (!bNeedsTick)
	{
		OutResult.Add(EAKValidationSeverity::Warning,
			LOCTEXT("AreaNoTick", "AreaEffect 需要勾选 NeedsTick 才能周期生效。"),
			const_cast<UAKAbilityTask_AreaEffect*>(this));
	}
}

void UAKAbilityTask_AreaEffect::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);

	OutJson->SetNumberField(TEXT("radius"), Radius);
	OutJson->SetBoolField(TEXT("followOwner"), bFollowOwner);
	OutJson->SetNumberField(TEXT("interval"), Interval);
	OutJson->SetNumberField(TEXT("maxTargetsPerTick"), MaxTargetsPerTick);
	OutJson->SetBoolField(TEXT("requireAbilitySystem"), bRequireAbilitySystem);
	OutJson->SetBoolField(TEXT("applyOncePerTarget"), bApplyOncePerTarget);

	TArray<TSharedPtr<FJsonValue>> FilterArray;
	UAKQueryFilter::ExportFilterArray(Filters, FilterArray);
	OutJson->SetArrayField(TEXT("filters"), FilterArray);
	OutJson->SetStringField(TEXT("effectClass"), EffectClass ? EffectClass->GetPathName() : TEXT(""));
	OutJson->SetNumberField(TEXT("effectLevel"), EffectLevel);
	OutJson->SetStringField(TEXT("cueTag"), CueTag.ToString());
}

void UAKAbilityTask_AreaEffect::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	double OutRadius = Radius;
	if (InJson.TryGetNumberField(TEXT("radius"), OutRadius))
	{
		Radius = static_cast<float>(OutRadius);
	}

	InJson.TryGetBoolField(TEXT("followOwner"), bFollowOwner);

	double OutInterval = Interval;
	if (InJson.TryGetNumberField(TEXT("interval"), OutInterval))
	{
		Interval = FMath::Max(0.05f, static_cast<float>(OutInterval));
	}

	double OutMax = MaxTargetsPerTick;
	if (InJson.TryGetNumberField(TEXT("maxTargetsPerTick"), OutMax))
	{
		MaxTargetsPerTick = FMath::Max(0, static_cast<int32>(OutMax));
	}

	InJson.TryGetBoolField(TEXT("requireAbilitySystem"), bRequireAbilitySystem);
	InJson.TryGetBoolField(TEXT("applyOncePerTarget"), bApplyOncePerTarget);

	const TArray<TSharedPtr<FJsonValue>>* FilterArray = nullptr;
	if (InJson.TryGetArrayField(TEXT("filters"), FilterArray) && FilterArray)
	{
		UAKQueryFilter::ImportFilterArray(*FilterArray, GetOuter(), Filters);
	}

	FString EffectPath;
	if (InJson.TryGetStringField(TEXT("effectClass"), EffectPath) && !EffectPath.IsEmpty())
	{
		EffectClass = LoadClass<UGameplayEffect>(nullptr, *EffectPath);
	}

	double OutLevel = EffectLevel;
	if (InJson.TryGetNumberField(TEXT("effectLevel"), OutLevel))
	{
		EffectLevel = static_cast<float>(OutLevel);
	}

	FString CueName;
	if (InJson.TryGetStringField(TEXT("cueTag"), CueName))
	{
		CueTag = FGameplayTag::RequestGameplayTag(FName(*CueName), /*bErrorIfNotFound=*/false);
	}
}

#undef LOCTEXT_NAMESPACE
