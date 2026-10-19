#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "AAKProjectile.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;
class USphereComponent;

/**
 * 最简投射物：服务端权威，逐帧扫掠推进，命中后施加 GE 与 Cue。
 *
 * 设计原则（与 AbilityKit 的投射物模块一致、但不自建同步）：
 * - 命中效果一律落到 GAS（GE + Cue），因此自动获得预测、复制与属性结算；
 * - 移动与命中检测用引擎的扫掠查询，不自建物理；
 * - 生成时机由 SpawnProjectile 任务控制在时间轴上，命中后的"再来一次伤害/Buff"由 GE 决定。
 */
UCLASS()
class UEABILITYKIT_API AAKProjectile : public AActor
{
	GENERATED_BODY()

public:
	AAKProjectile();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** 由 SpawnProjectile 任务调用：注入来源与命中效果 */
	void InitializeProjectile(AActor* InInstigator,
		UAbilitySystemComponent* InSourceASC,
		TSubclassOf<UGameplayEffect> InHitEffectClass,
		FGameplayTag InHitCueTag,
		float InEffectLevel,
		float InSpeed);

	/** 发射：给定世界方向 */
	void Fire(const FVector& InDirection);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	USphereComponent* CollisionComponent = nullptr;

	/** 扫掠半径（同时也是碰撞体半径） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Radius = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float GravityScale = 0.f;

	/** 最长存活时间，超时自动销毁 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float MaxLifetime = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	/** 命中后是否销毁 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	bool bDestroyOnHit = true;

	/** 可穿透的目标数量（0 = 命中即停） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxPierceCount = 0;

	/** 是否要求目标拥有 ASC（过滤掉桌椅板凳） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	bool bRequireAbilitySystem = true;

	/** 飞行中的 Cue（持续表现） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FGameplayTag FlightCueTag;

private:
	void ApplyHit(AActor* HitActor);
	void Expire();

	UPROPERTY()
	AActor* InstigatorActor = nullptr;

	UPROPERTY()
	UAbilitySystemComponent* SourceASC = nullptr;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> HitEffectClass;

	UPROPERTY()
	FGameplayTag HitCueTag;

	UPROPERTY()
	float EffectLevel = 1.f;

	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY()
	float Speed = 1000.f;

	UPROPERTY()
	float ElapsedTime = 0.f;

	UPROPERTY()
	TArray<AActor*> HitActors;
};
