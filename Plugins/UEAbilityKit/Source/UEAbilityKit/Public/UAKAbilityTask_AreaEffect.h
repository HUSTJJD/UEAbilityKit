#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UAKAbilityTaskBase.h"
#include "UAKQueryFilter.h"
#include "UAKAbilityTask_AreaEffect.generated.h"

class UGameplayEffect;

/** 区域效果的运行时状态（ScratchPad 演示：任务无状态，状态外置） */
UCLASS()
class UEABILITYKIT_API UAKAreaEffectScratchPad : public UObject
{
	GENERATED_BODY()

public:
	/** 距离下次生效的计时 */
	UPROPERTY()
	float Timer = 0.f;

	/** 已命中过的目标（配合 bApplyOncePerTarget） */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> HitActors;

	/** bFollowOwner = false 时固定的区域中心 */
	UPROPERTY()
	FVector FixedCenter = FVector::ZeroVector;

	UPROPERTY()
	bool bCenterCaptured = false;
};

/**
 * 区域效果：在持续时间（Duration）内周期性对区域内目标施加 GE。
 *
 * 与 AbilityKit 的区别：它自建"持续效果运行时"，我们把这件事故给 GAS——
 * 持续、周期、打断、移除全由 GE 的 Duration/Period/Tag 规则负责；
 * 本任务只处理"空间上哪些目标该被影响"，然后把 GE 打上去。
 */
UCLASS(meta = (DisplayName = "Area Effect"))
class UEABILITYKIT_API UAKAbilityTask_AreaEffect : public UAKAbilityTaskBase
{
	GENERATED_BODY()

public:
	UAKAbilityTask_AreaEffect();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Radius = 200.f;

	/** 区域中心相对施法者的偏移（本地空间） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	FVector Offset = FVector::ZeroVector;

	/** 区域是否跟随施法者移动 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	bool bFollowOwner = false;

	/** 每隔多久生效一次（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float Interval = 0.5f;

	/** 每次生效最多影响几个目标，0 表示不限 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxTargetsPerTick = 0;

	/** 是否要求目标拥有 ASC */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	bool bRequireAbilitySystem = true;

	/** 结果过滤器（与形状解耦，可自由组合） */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Area|Filter")
	TArray<UAKQueryFilter*> Filters;

	/** 每个目标只吃一次（否则每个 Interval 都会吃到） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	bool bApplyOncePerTarget = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	TSubclassOf<UGameplayEffect> EffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EffectLevel = 1.f;

	/** 每次生效时触发的表现 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FGameplayTag CueTag;

	virtual UObject* CreateScratchPad(const FAKAbilityContext& Context) override;
	virtual void OnTaskStart(FAKAbilityContext& Context) override;
	virtual void OnTaskTick(FAKAbilityContext& Context, float DeltaTime) override;
	virtual void GetGASImpact(FAKGASImpact& OutImpact) const override;
	virtual void Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const override;
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;

private:
	void ApplyOnce(FAKAbilityContext& Context);
	FVector GetCenter(const FAKAbilityContext& Context) const;
};
