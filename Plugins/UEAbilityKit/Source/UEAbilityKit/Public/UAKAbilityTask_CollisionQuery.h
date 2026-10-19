#pragma once

#include "CoreMinimal.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTask_CollisionQuery.generated.h"

/** 查询形状 */
UENUM(BlueprintType)
enum class EAKQueryShape : uint8
{
	Sphere	UMETA(DisplayName = "球"),
	Box		UMETA(DisplayName = "盒"),
	Capsule	UMETA(DisplayName = "胶囊")
};

/**
 * 碰撞查询：把命中的 Actor 写进上下文的目标集合，供后续 ApplyGameplayEffect 等任务使用。
 *
 * 只做"查询"，不做"命中处理"——命中后发生什么由后面的效果任务决定，
 * 这样同一个查询可以被伤害、治疗、上 Buff 等多种任务复用（AbilityKit 的原子能力思路）。
 */
UCLASS(meta = (DisplayName = "Collision Query"))
class UEABILITYKIT_API UAKAbilityTask_CollisionQuery : public UAKAbilityTaskBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
	EAKQueryShape Shape = EAKQueryShape::Sphere;

	/** 球半径 / 胶囊半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Radius = 100.f;

	/** 盒半尺寸 / 胶囊半高 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query", meta = (ClampMin = "0.0", UIMin = "0.0"))
	FVector Extent = FVector(50.f, 50.f, 50.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HalfHeight = 50.f;

	/** 相对于施法者的偏移（会随朝向旋转） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	/** true = 追加到已有目标；false = 先清空再写入 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
	bool bAppendToTargets = false;

	/** 最多保留多少个目标，0 表示不限 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxTargets = 0;

	/** 是否要求目标拥有 ASC（过滤掉桌椅板凳） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
	bool bRequireAbilitySystem = true;

	virtual void OnTaskStart(FAKAbilityContext& Context) override;
	virtual void GetGASImpact(FAKGASImpact& OutImpact) const override;
	virtual void Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const override;
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;
};
