#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UAKTypes.h"
#include "UAKAbilityTrack.generated.h"

class UAKAbilityTaskBase;

/**
 * 轨道（Track）：时间轴上的一级容器。
 *
 * 与 Able 的关键差异：Able 是 Task 平铺 + 按类型过滤；UAK 用显式的轨道容器承载 Task，
 * 因此可以对一整类事情做整体开关（例如静音「表现」轨道，只跑逻辑做服务器校验）。
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class UEABILITYKIT_API UAKAbilityTrack : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	FName TrackName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	EAKTrackType TrackType = EAKTrackType::Custom;

	/** 关闭后该轨道上的任务全部不参与执行 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	bool bEnabled = true;

	/** 仅编辑器：折叠显示 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	bool bCollapsed = false;

	/** 轨道上的任务片段（内联实例化） */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Track")
	TArray<UAKAbilityTaskBase*> Tasks;

	/** 轨道显示名：未命名时回落到类型名 */
	FText GetDisplayName() const;
};
