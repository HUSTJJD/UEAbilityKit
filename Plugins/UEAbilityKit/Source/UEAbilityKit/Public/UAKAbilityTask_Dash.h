#pragma once

#include "CoreMinimal.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTask_Dash.generated.h"

/**
 * 位移（冲刺）。
 *
 * 用 ACharacter::LaunchCharacter 施加一次性速度，不自建移动系统——
 * 移动、碰撞求解、网络同步全部交给引擎的 CharacterMovement。
 */
UCLASS(meta = (DisplayName = "Dash"))
class UEABILITYKIT_API UAKAbilityTask_Dash : public UAKAbilityTaskBase
{
	GENERATED_BODY()

public:
	/** 相对施法者朝向的位移方向（本地空间） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	FVector LocalDirection = FVector(1.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Strength = 1200.f;

	/** 是否覆盖水平速度 / 垂直速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	bool bOverrideHorizontal = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	bool bOverrideVertical = false;

	/** 目标为世界空间方向而非本地方向（例如击退） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	bool bWorldSpace = false;

	virtual void OnTaskStart(FAKAbilityContext& Context) override;
	virtual void GetGASImpact(FAKGASImpact& OutImpact) const override;
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;
};
