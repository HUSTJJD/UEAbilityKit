#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UAKCondition.h"
#include "UAKChanneling.generated.h"

struct FAKAbilityContext;

/**
 * 引导 / 吟唱条件：在引导窗口内**持续**检查，任一条件不满足即触发失败动作。
 *
 * 与 WaitUntil 的区别：
 * - WaitUntil：条件**满足**就继续（等一件事发生）；
 * - Channeling：条件**不满足**就打断（维持一件事成立）。
 *
 * 与 Branch 的区别：Branch 是单次判定开关轨道，Channeling 是每帧维持。
 *
 * 学自 AbleSP 的 Channeling 设计，但落点换成 GAS：
 * 条件优先用 Tag / 属性（GAS 原生），输入与空间条件作为可扩展的内置类型。
 */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class UEABILITYKIT_API UAKChannelingCondition : public UObject
{
	GENERATED_BODY()

public:
	/** 返回 true 表示条件仍然成立 */
	virtual bool IsSatisfied(const FAKAbilityContext& Context) const { return true; }

	/**
	 * 条件失败时是否需要通知服务器取消。
	 * 客户端本地判定（例如输入松手）通常需要，纯本地表现的条件不需要。
	 */
	virtual bool RequiresServerNotification() const { return false; }

#if WITH_EDITOR
	virtual FText GetConditionDescription() const;
#endif
};

/** 基于 GAS 的 Tag / 属性（与 Branch、WaitUntil 共用 FAKCondition，语义一致） */
UCLASS(meta = (DisplayName = "Tag / 属性"))
class UEABILITYKIT_API UAKChanneling_TagCondition : public UAKChannelingCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Channeling")
	FAKCondition Condition;

	virtual bool IsSatisfied(const FAKAbilityContext& Context) const override;
};

/** 站桩吟唱：移动速度超过阈值即打断 */
UCLASS(meta = (DisplayName = "移动速度"))
class UEABILITYKIT_API UAKChanneling_MovementCondition : public UAKChannelingCondition
{
	GENERATED_BODY()

public:
	/** 速度阈值（cm/s） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Channeling", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpeedThreshold = 10.f;

	/** 只算水平速度（XY） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Channeling")
	bool bUseXYSpeedOnly = true;

	virtual bool IsSatisfied(const FAKAbilityContext& Context) const override;
};

/** 距离维持：与第一个目标的距离超过阈值即打断 */
UCLASS(meta = (DisplayName = "距离"))
class UEABILITYKIT_API UAKChanneling_DistanceCondition : public UAKChannelingCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Channeling", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DistanceThreshold = 500.f;

	/** 只算水平距离 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Channeling")
	bool bUse2DDistance = false;

	/** 计入双方胶囊半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Channeling")
	bool bUseCapsuleRadius = true;

	/** 没有目标时视为成立（避免无人可锁时立刻打断） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Channeling")
	bool bPassWhenNoTarget = true;

	virtual bool IsSatisfied(const FAKAbilityContext& Context) const override;
};

/**
 * 自定义条件：交给项目 / 蓝图子类实现。
 *
 * 典型用途：按住按键（输入松手即打断）——输入绑定是项目相关的，
 * UAK 不把它做成内置类型，避免把输入系统钉死在框架里。
 */
UCLASS(Blueprintable, meta = (DisplayName = "自定义"))
class UEABILITYKIT_API UAKChanneling_CustomCondition : public UAKChannelingCondition
{
	GENERATED_BODY()

public:
	/** 需要通知服务器（输入类条件通常是客户端先感知） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Channeling")
	bool bRequiresServerNotification = true;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Channeling")
	bool IsSatisfiedBP(const FAKAbilityContext& Context) const;

	virtual bool IsSatisfied(const FAKAbilityContext& Context) const override;
	virtual bool RequiresServerNotification() const override { return bRequiresServerNotification; }
};
