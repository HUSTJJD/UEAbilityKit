#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTask_SendGameplayCue.generated.h"

/** GameplayCue 操作类型 */
UENUM(BlueprintType)
enum class EAKCueOperation : uint8
{
	/** 一次性表现（爆炸、命中特效） */
	Execute	UMETA(DisplayName = "Execute (一次性)"),
	/** 持续型表现的开始（灼烧、护盾） */
	Add		UMETA(DisplayName = "Add (持续开始)"),
	/** 持续型表现的结束 */
	Remove	UMETA(DisplayName = "Remove (持续结束)")
};

/**
 * 发送 GameplayCue（表现层）。
 *
 * 逻辑与表现严格分离：伤害/状态变化走 GE，这里只负责"看到/听到什么"。
 * 因此该任务默认只在本地端执行（ClientOnly）。
 */
UCLASS(meta = (DisplayName = "Send Gameplay Cue"))
class UEABILITYKIT_API UAKAbilityTask_SendGameplayCue : public UAKAbilityTaskBase
{
	GENERATED_BODY()

public:
	UAKAbilityTask_SendGameplayCue();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue")
	FGameplayTag CueTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue")
	EAKCueOperation Operation = EAKCueOperation::Execute;

	virtual void OnTaskStart(FAKAbilityContext& Context) override;
	virtual void GetGASImpact(FAKGASImpact& OutImpact) const override;
	virtual void Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const override;
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;
};
