#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "UAKQueryFilter.generated.h"

class AActor;

/**
 * 查询结果过滤器：与形状解耦，可自由组合。
 *
 * 为什么与形状分开（学自 AbleSP 的 Targeting/Filters 设计）：
 * 形状回答"在哪查"，过滤器回答"哪些算数"。
 * 两者分开后可以任意组合（锥形 + 只要带 Tag 的 + 30m 内 + 有视线），
 * 而不用为每种组合各写一个查询任务。
 */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class UEABILITYKIT_API UAKQueryFilter : public UObject
{
	GENERATED_BODY()

public:
	/** 返回 true 表示保留该候选目标 */
	virtual bool Passes(const AActor* Candidate, const AActor* Source) const { return true; }

	/** Json 双向（子类必须调用 Super，并写入与 ImportJson 对称的字段） */
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const;
	virtual void ImportJson(const FJsonObject& InJson);

	/** 过滤器数组的序列化辅助（供查询类任务复用） */
	static void ExportFilterArray(const TArray<UAKQueryFilter*>& Filters, TArray<TSharedPtr<FJsonValue>>& OutArray);
	static void ImportFilterArray(const TArray<TSharedPtr<FJsonValue>>& InArray, UObject* Outer, TArray<UAKQueryFilter*>& OutFilters);

#if WITH_EDITOR
	virtual FText GetFilterDescription() const;
#endif
};

/** 按目标 ASC 上的 GameplayTag 过滤 */
UCLASS(meta = (DisplayName = "Tag 过滤"))
class UEABILITYKIT_API UAKQueryFilter_Tag : public UAKQueryFilter
{
	GENERATED_BODY()

public:
	/** 必须全部拥有 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	FGameplayTagContainer RequiredTags;

	/** 拥有任一即排除 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	FGameplayTagContainer BlockedTags;

	/** 目标没有 ASC 时是否保留（false 会顺带过滤掉非战斗对象） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	bool bKeepIfNoAbilitySystem = false;

	virtual bool Passes(const AActor* Candidate, const AActor* Source) const override;
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;
};

/** 按距离过滤 */
UCLASS(meta = (DisplayName = "距离过滤"))
class UEABILITYKIT_API UAKQueryFilter_Distance : public UAKQueryFilter
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxDistance = 500.f;

	/** 只算水平距离（XY 平面） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	bool bUse2DDistance = false;

	/** 计入双方胶囊半径，避免大体型目标被误判超距 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	bool bUseCapsuleRadius = false;

	virtual bool Passes(const AActor* Candidate, const AActor* Source) const override;
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;
};

/** 视线检测：中间有遮挡就排除 */
UCLASS(meta = (DisplayName = "视线过滤"))
class UEABILITYKIT_API UAKQueryFilter_LineOfSight : public UAKQueryFilter
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** 从眼睛高度而非脚底发射射线 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	bool bTraceFromEyeHeight = true;

	virtual bool Passes(const AActor* Candidate, const AActor* Source) const override;
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;
};
