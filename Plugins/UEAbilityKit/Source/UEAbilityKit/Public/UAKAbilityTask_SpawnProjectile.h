#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTask_SpawnProjectile.generated.h"

class AAKProjectile;
class UGameplayEffect;

/**
 * 生成投射物。
 *
 * 默认 ServerOnly：投射物由服务端权威生成与推进，客户端表现交给 Cue。
 * 命中效果建模为 GE，因此伤害/属性/Buff 的结算与复制全部由 GAS 负责。
 */
UCLASS(meta = (DisplayName = "Spawn Projectile"))
class UEABILITYKIT_API UAKAbilityTask_SpawnProjectile : public UAKAbilityTaskBase
{
	GENERATED_BODY()

public:
	UAKAbilityTask_SpawnProjectile();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	TSubclassOf<AAKProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float Speed = 1500.f;

	/** 生成点相对施法者的偏移（本地空间） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FVector SpawnOffset = FVector(80.f, 0.f, 60.f);

	/** 骨骼插槽名（留空则用偏移） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FName SocketName = NAME_None;

	/** 有目标时朝第一个目标飞；否则朝施法者正前方 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	bool bAimAtFirstTarget = true;

	/** 一次生成几个（配合 SpreadAngle 做扇形弹幕） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "1", UIMin = "1"))
	int32 SpawnCount = 1;

	/** 多发之间的水平散布角（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "180.0"))
	float SpreadAngle = 0.f;

	// ── 命中效果（落到 GAS）────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	TSubclassOf<UGameplayEffect> HitEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	FGameplayTag HitCueTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EffectLevel = 1.f;

	virtual void OnTaskStart(FAKAbilityContext& Context) override;
	virtual void GetGASImpact(FAKGASImpact& OutImpact) const override;
	virtual void Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const override;
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;

private:
	FVector ComputeSpawnLocation(const FAKAbilityContext& Context) const;
	FVector ComputeDirection(const FAKAbilityContext& Context, const FVector& Origin) const;
};
