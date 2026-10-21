#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTask_ApplyLooseTags.generated.h"

/**
 * 在任务的时间窗内给 ASC 挂上 Loose GameplayTag，任务结束时自动摘掉。
 *
 * 典型用途：霸体、无敌、蓄力中、不可被选取——这些"限时状态标记"本质上就是
 * 一个带时间窗的 Tag，用 GE 去做反而是绕远路（还要配 Duration policy 与授予 Tag）。
 *
 * 为什么不直接用 GE：
 * GE 的 Duration 与服务器时间、预测回滚绑定，适合"会互相覆盖、需要堆叠"的状态；
 * 而"技能动画这一段时间我就是霸体"是**编排语义**，它应该跟时间轴对齐，
 * 打断/中断时跟任务一起收尾——这正是本任务做的事。
 */
UCLASS(meta = (DisplayName = "Apply Loose Tags"))
class UEABILITYKIT_API UAKAbilityTask_ApplyLooseTags : public UAKAbilityTaskBase
{
	GENERATED_BODY()

public:
	UAKAbilityTask_ApplyLooseTags();

	/** 任务存活期间挂上的 Tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	FGameplayTagContainer Tags;

	/** 是否复制到其他端（霸体这类影响判定的标记需要；纯本地表现标记可关） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	bool bReplicate = true;

	/**
	 * 被中断时是否仍然移除。
	 * 关掉意味着"被打断后 Tag 会残留"——只有当你有另一条清理路径时才这么做。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	bool bRemoveOnInterrupt = true;

	virtual void OnTaskStart(FAKAbilityContext& Context) override;
	virtual void OnTaskEnd(FAKAbilityContext& Context, bool bInterrupted) override;

	virtual void GetGASImpact(FAKGASImpact& OutImpact) const override;
	virtual void Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const override;
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const override;
	virtual void ImportJson(const FJsonObject& InJson) override;
};
