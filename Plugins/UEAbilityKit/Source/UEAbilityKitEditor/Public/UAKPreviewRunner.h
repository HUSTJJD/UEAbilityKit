#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UAKPreviewRunner.generated.h"

class AActor;
class UAbilitySystemComponent;
class UAKAbilityAsset;
class UAKAbilityInstance;
class UAKGameplayAbility;
class UAKTrace;

/**
 * 编辑器预览里的"一次施放"。
 *
 * 生命周期就是技能本身：Start() → 每帧 Tick() → 时间轴跑到 Length 自动结束。
 * 不跑关卡、不 Possess、不联网 —— 那属于 PIE 的职责。这里只做一件事：
 * 让技能在预览世界里真的发生一次（Montage / GameplayCue / 投射物 / 位移都算）。
 *
 * 与真实施放的差别只有两处，都是刻意的：
 * - 不走 GAS 的激活流程（不检查冷却 / 消耗 / Tag 互斥）—— 预览要能反复放；
 * - 两端标记都置 true，让 ClientOnly 的表现类任务不会被 ShouldRunOnThisEnd() 滤掉。
 *
 * 副作用的处理：每次 Start() 由调用方重建预览角色（属性 / GE 天然归零），
 * Stop()/Reset() 再收掉 Montage 与 Cue，不留残留在预览世界里。
 */
UCLASS()
class UEABILITYKITEDITOR_API UUAKPreviewRunner : public UObject
{
	GENERATED_BODY()

public:
	/** 开始一次施放。预览角色必须先就位（带 ASC，没有就现场挂一个）。 */
	bool Start(AActor* InPreviewActor, UAKAbilityAsset* InAsset, bool bInLoop);

	/** 每帧推进；返回是否仍在运行。 */
	bool Tick(float DeltaTime);

	/** 中断这次施放（等价于技能被打断：走 Finish(true) 的中断语义）。 */
	void Stop();

	/** 结束并清理副作用（Montage、Cue、临时对象）。 */
	void Reset();

	bool IsRunning() const;
	float GetCurrentTime() const;
	float GetLength() const;

private:
	/** 角色自带 ASC 就用它的，没有就挂一个 transient 的（预览世界没有 GameMode 帮我们做）。 */
	UAbilitySystemComponent* EnsureAbilitySystem(AActor* Actor);

	void Restart();

	UPROPERTY()
	TObjectPtr<AActor> PreviewActor;

	UPROPERTY()
	TObjectPtr<UAKAbilityAsset> Asset;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> ASC;

	/**
	 * 仅供任务记账用的 transient 技能对象（不授予、不激活）。
	 * PlayMontage 走的是 ASC->PlayMontage(Ability, ...)，需要一个非空 Ability。
	 */
	UPROPERTY()
	TObjectPtr<UAKGameplayAbility> PreviewAbility;

	UPROPERTY()
	TObjectPtr<UAKAbilityInstance> Instance;

	bool bLoop = false;
};
