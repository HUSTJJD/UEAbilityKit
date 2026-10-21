#pragma once

#include "CoreMinimal.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTask_FaceTarget.generated.h"

/** 面朝哪里 */
UENUM(BlueprintType)
enum class EAKFaceMode : uint8
{
	/** 面向上下文里的第一个目标（目标由 CollisionQuery / TargetData 写入） */
	FirstTarget		UMETA(DisplayName = "第一个目标"),
	/** 面向施法者当前的移动方向 */
	Velocity		UMETA(DisplayName = "移动方向"),
	/** 面向控制器的朝向（玩家视角） */
	ControlRotation	UMETA(DisplayName = "控制器朝向"),
	/** 面向一个固定的世界方向（击退、固定演出） */
	WorldDirection	UMETA(DisplayName = "世界方向")
};

/** 平滑转向的运行时状态 */
UCLASS()
class UEABILITYKIT_API UAKFaceTargetScratchPad : public UObject
{
	GENERATED_BODY()

public:
	FRotator TargetRotation = FRotator::ZeroRotator;
	bool bHasTarget = false;
};

/**
 * 转向（近战与演出最常用的一个动作）。
 *
 * 刻意做得很小：只负责"把朝向改到某个方向"，不碰移动、不碰动画、不碰网络——
 * 角色朝向的复制由引擎的 Movement / Actor 复制负责，这里只调用 SetActorRotation。
 */
UCLASS(meta = (DisplayName = "Face Target"))
class UEABILITYKIT_API UAKAbilityTask_FaceTarget : public UAKAbilityTaskBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	EAKFaceMode Mode = EAKFaceMode::FirstTarget;

	/** 仅在 Mode = WorldDirection 时生效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (EditCondition = "Mode == EAKFaceMode::WorldDirection"))
	FVector WorldDirection = FVector(1.f, 0.f, 0.f);

	/** true：进入任务就转到位；false：按 TurnSpeed 平滑转（需要勾选 NeedsTick） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	bool bInstant = true;

	/** 平滑转向的角速度（度 / 秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (ClampMin = "1.0", UIMin = "1.0", EditCondition = "!bInstant"))
	float TurnSpeed = 720.f;

	/** 只改 Yaw（角色转向的常态）；关掉则连 Pitch 一起对齐（飞行/俯仰演出） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	bool bOnlyYaw = true;

	/** 没有可用目标时是否放弃转向（否则保持当前朝向） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	bool bCancelWhenNoTarget = false;

	/** 没有目标且勾选了 CancelWhenNoTarget 时提前结束（不再空转） */
	virtual bool ShouldEndEarly(const FAKAbilityContext& Context) const override;

	virtual void OnTaskStart(FAKAbilityContext& Context) override;
	virtual void OnTaskTick(FAKAbilityContext& Context, float DeltaTime) override;
	virtual UObject* CreateScratchPad(const FAKAbilityContext& Context) override;
	virtual void Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const override;
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;

private:
	/** 算出目标朝向；拿不到时返回 false */
	bool ComputeTargetRotation(const FAKAbilityContext& Context, FRotator& OutRotation) const;

	void ApplyRotation(AActor* Owner, const FRotator& TargetRotation) const;
};
