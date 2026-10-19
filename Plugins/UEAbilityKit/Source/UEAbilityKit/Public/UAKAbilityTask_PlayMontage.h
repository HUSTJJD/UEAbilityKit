#pragma once

#include "CoreMinimal.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTask_PlayMontage.generated.h"

class UAnimMontage;

/**
 * 播放 AnimMontage（表现层）。
 *
 * 走 ASC->PlayMontage 而不是直接 AnimInstance->Montage_Play，
 * 这样蒙太奇会进入 GAS 的管理范围（复制、打断、CurrentMontage 查询）。
 */
UCLASS(meta = (DisplayName = "Play Montage"))
class UEABILITYKIT_API UAKAbilityTask_PlayMontage : public UAKAbilityTaskBase
{
	GENERATED_BODY()

public:
	UAKAbilityTask_PlayMontage();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	UAnimMontage* Montage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float Rate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	FName StartSection = NAME_None;

	/** 任务结束时是否停止该蒙太奇 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	bool bStopOnEnd = false;

	virtual void OnTaskStart(FAKAbilityContext& Context) override;
	virtual void OnTaskEnd(FAKAbilityContext& Context, bool bInterrupted) override;
	virtual void Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const override;
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;
};
