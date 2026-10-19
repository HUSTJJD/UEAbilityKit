#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "UAKTargeting.generated.h"

struct FAKAbilityContext;

/**
 * 目标选择适配：把 GAS 的 FGameplayAbilityTargetDataHandle 写进 UAK 的上下文。
 *
 * UAK 不自建目标选择系统——目标来自 GAS 的 TargetData（由项目用 WaitTargetData /
 * 自定义 Targeting Actor / 服务器下发等方式产生），这里只做"取出来放进 Context"。
 */
UCLASS()
class UEABILITYKIT_API UAKTargeting : public UObject
{
	GENERATED_BODY()

public:
	/** 把 TargetData 中的 Actor 追加到上下文目标集合 */
	static void AppendTargetDataToContext(const FGameplayAbilityTargetDataHandle& Handle, FAKAbilityContext& Context);

	/** 把一批 Actor 追加到上下文目标集合 */
	static void AppendActorsToContext(const TArray<TWeakObjectPtr<AActor>>& Actors, FAKAbilityContext& Context);
};
