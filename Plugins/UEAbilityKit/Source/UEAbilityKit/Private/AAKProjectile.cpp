#include "AAKProjectile.h"

#include "UAKQuery.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"

AAKProjectile::AAKProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bReplicates = true;
	// 投射物由服务端权威生成与推进，客户端只做表现
	SetReplicateMovement(true);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComponent->SetSphereRadius(Radius);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = CollisionComponent;
}

void AAKProjectile::BeginPlay()
{
	Super::BeginPlay();

	CollisionComponent->SetSphereRadius(Radius);

	if (FlightCueTag.IsValid() && SourceASC)
	{
		SourceASC->AddGameplayCue(FlightCueTag);
	}
}

void AAKProjectile::InitializeProjectile(AActor* InInstigator,
	UAbilitySystemComponent* InSourceASC,
	TSubclassOf<UGameplayEffect> InHitEffectClass,
	FGameplayTag InHitCueTag,
	float InEffectLevel,
	float InSpeed)
{
	InstigatorActor = InInstigator;
	SourceASC = InSourceASC;
	HitEffectClass = InHitEffectClass;
	HitCueTag = InHitCueTag;
	EffectLevel = InEffectLevel;
	Speed = FMath::Max(1.f, InSpeed);
	ElapsedTime = 0.f;
	HitActors.Reset();
}

void AAKProjectile::Fire(const FVector& InDirection)
{
	Velocity = InDirection.GetSafeNormal() * Speed;
	SetActorRotation(Velocity.GetSafeNormal().Rotation());
}

void AAKProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ElapsedTime += DeltaTime;
	if (ElapsedTime >= MaxLifetime)
	{
		Expire();
		return;
	}

	if (GravityScale != 0.f)
	{
		Velocity += FVector(0.f, 0.f, -980.f * GravityScale) * DeltaTime;
	}

	const FVector Start = GetActorLocation();
	const FVector Delta = Velocity * DeltaTime;
	const FVector End = Start + Delta;

	if (Delta.IsNearlyZero())
	{
		return;
	}

	FHitResult Hit;
	const bool bHit = UAKQuery::SweepTo(GetWorld(), Start, End, Radius, TraceChannel, InstigatorActor, Hit);

	if (bHit)
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && !HitActors.Contains(HitActor))
		{
			ApplyHit(HitActor);
			HitActors.Add(HitActor);

			const bool bCanPierce = HitActors.Num() <= MaxPierceCount;
			if (!bCanPierce || bDestroyOnHit)
			{
				SetActorLocation(Hit.ImpactPoint);
				Expire();
				return;
			}
		}
	}

	SetActorLocation(bHit ? Hit.ImpactPoint : End);
	SetActorRotation(Velocity.GetSafeNormal().Rotation());
}

void AAKProjectile::ApplyHit(AActor* HitActor)
{
	if (!HitActor || !SourceASC)
	{
		return;
	}

	if (bRequireAbilitySystem)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		if (!TargetASC)
		{
			return;
		}
	}

	// 命中表现（由 GAS 的 Cue 系统负责复制与预测处理）
	if (HitCueTag.IsValid())
	{
		SourceASC->ExecuteGameplayCue(HitCueTag);
	}

	// 命中效果（伤害 / Buff / 治疗 … 一律建模为 GE）
	if (HitEffectClass)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		if (TargetASC)
		{
			const FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
			const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(HitEffectClass, EffectLevel, EffectContext);
			if (SpecHandle.IsValid())
			{
				SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
			}
		}
	}
}

void AAKProjectile::Expire()
{
	if (FlightCueTag.IsValid() && SourceASC)
	{
		SourceASC->RemoveGameplayCue(FlightCueTag);
	}

	Destroy();
}
