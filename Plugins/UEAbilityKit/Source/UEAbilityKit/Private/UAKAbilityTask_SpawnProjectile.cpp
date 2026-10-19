#include "UAKAbilityTask_SpawnProjectile.h"

#include "AAKProjectile.h"
#include "UAKAbilityAsset.h"
#include "UAKAbilityContext.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "UAKAbilityTask_SpawnProjectile"

UAKAbilityTask_SpawnProjectile::UAKAbilityTask_SpawnProjectile()
{
	// 投射物由服务端权威生成
	NetMode = EAKTaskNetMode::ServerOnly;
}

FVector UAKAbilityTask_SpawnProjectile::ComputeSpawnLocation(const FAKAbilityContext& Context) const
{
	AActor* Owner = Context.Owner.Get();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}

	if (!SocketName.IsNone())
	{
		if (USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
		{
			return Mesh->GetSocketLocation(SocketName);
		}
	}

	return Owner->GetActorLocation() + Owner->GetActorRotation().RotateVector(SpawnOffset);
}

FVector UAKAbilityTask_SpawnProjectile::ComputeDirection(const FAKAbilityContext& Context, const FVector& Origin) const
{
	AActor* Owner = Context.Owner.Get();
	if (!Owner)
	{
		return FVector::ForwardVector;
	}

	if (bAimAtFirstTarget && Context.Targets.Num() > 0 && Context.Targets[0].IsValid())
	{
		AActor* Target = Context.Targets[0].Get();
		const FVector AimPoint = Target->GetActorLocation();
		return (AimPoint - Origin).GetSafeNormal();
	}

	return Owner->GetActorForwardVector();
}

void UAKAbilityTask_SpawnProjectile::OnTaskStart(FAKAbilityContext& Context)
{
	AActor* Owner = Context.Owner.Get();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World || !ProjectileClass)
	{
		return;
	}

	const FVector Origin = ComputeSpawnLocation(Context);
	const FVector BaseDirection = ComputeDirection(Context, Origin);
	const int32 Count = FMath::Max(1, SpawnCount);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		FVector Direction = BaseDirection;

		if (Count > 1 && SpreadAngle > 0.f)
		{
			// 以水平面内的均匀扇形散布
			const float Step = (Count > 1) ? (SpreadAngle / static_cast<float>(Count - 1)) : 0.f;
			const float Yaw = -SpreadAngle * 0.5f + Step * Index;
			Direction = Direction.RotateAngleAxis(Yaw, FVector::UpVector);
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Owner;
		SpawnParams.Instigator = Cast<APawn>(Owner);
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AAKProjectile* Projectile = World->SpawnActor<AAKProjectile>(
			ProjectileClass, Origin, Direction.Rotation(), SpawnParams);

		if (!Projectile)
		{
			continue;
		}

		Projectile->InitializeProjectile(Context.Instigator.Get(), Context.SourceASC.Get(),
			HitEffectClass, HitCueTag, EffectLevel, Speed);
		Projectile->Fire(Direction);
	}
}

void UAKAbilityTask_SpawnProjectile::GetGASImpact(FAKGASImpact& OutImpact) const
{
	if (HitEffectClass)
	{
		OutImpact.Effects.AddUnique(TSoftClassPtr<UGameplayEffect>(HitEffectClass));
	}
	if (HitCueTag.IsValid())
	{
		OutImpact.Cues.AddUnique(HitCueTag);
	}

	OutImpact.Summary = FText::Format(
		LOCTEXT("SpawnSummary", "生成 {0} 个投射物（速度 {1}）"),
		FText::AsNumber(FMath::Max(1, SpawnCount)), FText::AsNumber(Speed));
}

void UAKAbilityTask_SpawnProjectile::Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const
{
	Super::Validate(Asset, OutResult);

	if (!ProjectileClass)
	{
		OutResult.Add(EAKValidationSeverity::Error,
			LOCTEXT("NoProjectileClass", "SpawnProjectile 未指定投射物类。"),
			const_cast<UAKAbilityTask_SpawnProjectile*>(this));
	}
}

void UAKAbilityTask_SpawnProjectile::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);

	OutJson->SetStringField(TEXT("projectileClass"), ProjectileClass ? ProjectileClass->GetPathName() : TEXT(""));
	OutJson->SetNumberField(TEXT("speed"), Speed);
	OutJson->SetStringField(TEXT("socketName"), SocketName.ToString());
	OutJson->SetBoolField(TEXT("aimAtFirstTarget"), bAimAtFirstTarget);
	OutJson->SetNumberField(TEXT("spawnCount"), SpawnCount);
	OutJson->SetNumberField(TEXT("spreadAngle"), SpreadAngle);
	OutJson->SetStringField(TEXT("hitEffectClass"), HitEffectClass ? HitEffectClass->GetPathName() : TEXT(""));
	OutJson->SetStringField(TEXT("hitCueTag"), HitCueTag.ToString());
	OutJson->SetNumberField(TEXT("effectLevel"), EffectLevel);
}

void UAKAbilityTask_SpawnProjectile::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	FString ProjectilePath;
	if (InJson.TryGetStringField(TEXT("projectileClass"), ProjectilePath) && !ProjectilePath.IsEmpty())
	{
		ProjectileClass = LoadClass<AAKProjectile>(nullptr, *ProjectilePath);
	}

	double OutSpeed = Speed;
	if (InJson.TryGetNumberField(TEXT("speed"), OutSpeed))
	{
		Speed = static_cast<float>(OutSpeed);
	}

	FString Socket;
	if (InJson.TryGetStringField(TEXT("socketName"), Socket))
	{
		SocketName = FName(*Socket);
	}

	InJson.TryGetBoolField(TEXT("aimAtFirstTarget"), bAimAtFirstTarget);

	double OutCount = SpawnCount;
	if (InJson.TryGetNumberField(TEXT("spawnCount"), OutCount))
	{
		SpawnCount = FMath::Max(1, static_cast<int32>(OutCount));
	}

	double OutSpread = SpreadAngle;
	if (InJson.TryGetNumberField(TEXT("spreadAngle"), OutSpread))
	{
		SpreadAngle = static_cast<float>(OutSpread);
	}

	FString EffectPath;
	if (InJson.TryGetStringField(TEXT("hitEffectClass"), EffectPath) && !EffectPath.IsEmpty())
	{
		HitEffectClass = LoadClass<UGameplayEffect>(nullptr, *EffectPath);
	}

	FString CueName;
	if (InJson.TryGetStringField(TEXT("hitCueTag"), CueName))
	{
		HitCueTag = FGameplayTag::RequestGameplayTag(FName(*CueName), /*bErrorIfNotFound=*/false);
	}

	double OutLevel = EffectLevel;
	if (InJson.TryGetNumberField(TEXT("effectLevel"), OutLevel))
	{
		EffectLevel = static_cast<float>(OutLevel);
	}
}

#undef LOCTEXT_NAMESPACE
