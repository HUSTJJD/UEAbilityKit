#pragma once

#include "CoreMinimal.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet.h"

#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"
#include "UAKChanneling.h"
#include "UAKTypes.h"

#include "UAKTestCommon.generated.h"

class UAKAbilityAsset;
class UAKAbilityTrack;
class UAKAbilityInstance;
class UGameplayEffect;

// 直接用引擎的官方宏 ATTRIBUTE_ACCESSORS_BASIC（AttributeSet.h），不自己再包一层。
// 注意：老的 ATTRIBUTE_ACCESSORS 在 UE 5 已移除，现在只出现在引擎注释的文档示例里。

/**
 * 测试用属性集：只保留 Health / MaxHealth，够验证 GE 落地即可。
 * 放在测试模块里，避免测试依赖示例工程（UAKDemo）。
 */
UCLASS()
class UEABILITYKITTESTS_API UAKTestAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, Category = "Test")
	FGameplayAttributeData MaxHealth;

	ATTRIBUTE_ACCESSORS_BASIC(UAKTestAttributeSet, Health);
	ATTRIBUTE_ACCESSORS_BASIC(UAKTestAttributeSet, MaxHealth);
};

/**
 * 测试用 GAS 载体：一个带 ASC + 属性集的空 Actor。
 * 用它可以把「施法者 → 目标 → GE → 属性变化」整条链跑通，而不需要关卡与 PIE。
 */
UCLASS()
class UEABILITYKITTESTS_API AAKTestGASActor : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAKTestGASActor();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UAKTestAttributeSet> Attributes = nullptr;

	/** 根组件：有了它 SetActorLocation / GetActorLocation 才有意义 */
	UPROPERTY()
	TObjectPtr<USceneComponent> Root = nullptr;
};

/**
 * 探针任务：把生命周期事件写进一份全局日志，供「实例测试」断言。
 *
 * 为什么不用真实任务（例如 ApplyGE）来做时序断言：
 * 真实任务有副作用、依赖世界与资产，时序断言会被业务细节干扰。
 * 探针只回答一件事——"执行器在什么时间、以什么顺序、用什么方式调用了任务"。
 */
UCLASS()
class UEABILITYKITTESTS_API UAKAbilityTask_TestProbe : public UAKAbilityTaskBase
{
	GENERATED_BODY()

public:
	virtual void OnTaskStart(FAKAbilityContext& Context) override;
	virtual void OnTaskTick(FAKAbilityContext& Context, float DeltaTime) override;
	virtual void OnTaskEnd(FAKAbilityContext& Context, bool bInterrupted) override;

	/** 全局日志（测试之间用 ResetLog 清空） */
	static TArray<FString>& GetLog();
	static void ResetLog();
	static void AppendLog(const FString& Line);

	/** 便于断言：日志里是否包含某行 */
	static bool LogContains(const FString& SubString);
	static int32 CountLog(const FString& SubString);
};

/**
 * 测试用 GE：伤害（Health -30）。
 *
 * 为什么用 C++ 类而不是运行时 NewObject：
 * 任务里的 EffectClass 是 TSubclassOf<UGameplayEffect>，必须是 UClass；
 * 而 GE 是数据资产，运行时造不出新的 UClass。把 GE 写成 C++ 类，
 * 既能在无内容（CI 无 .uasset）的环境里跑，也是 GE 的官方用法之一。
 */
UCLASS()
class UEABILITYKITTESTS_API UAKTestDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAKTestDamageEffect();
};

/** 测试用 GE：治疗（Health +15） */
UCLASS()
class UEABILITYKITTESTS_API UAKTestHealEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAKTestHealEffect();
};

/** 测试用引导条件：结果由 bSatisfied 直接指定，便于断言"条件不成立 → 打断" */
UCLASS()
class UEABILITYKITTESTS_API UAKTestChannelingCondition : public UAKChannelingCondition
{
	GENERATED_BODY()

public:
	UPROPERTY()
	bool bSatisfied = true;

	virtual bool IsSatisfied(const FAKAbilityContext& Context) const override { return bSatisfied; }
};

/** 一个临时的编辑器预览世界：给需要 SpawnActor 的 GAS 测试用，析构时自动销毁 */
struct FUAKTestWorldScope
{
	UWorld* World = nullptr;

	/** 创建世界；失败时 World 为 nullptr */
	bool Create();
	~FUAKTestWorldScope();

private:
	bool bCreated = false;
};

namespace UAKTest
{
	/** 新建一条技能资产（默认长度 1s，非循环） */
	UAKAbilityAsset* NewAsset(float Length = 1.f);

	/**
	 * 去掉 Json 里的资产名（"asset": "AKAbilityAsset_24"）。
	 *
	 * NewAsset() 每次都会拿到一个不重名的资产，而导出文本里带着它的名字。
	 * 比较"往返是否一致"时它属于噪声；这里只摘掉这一项，
	 * 不去 Rename 资产——改名会撞上已存在的对象，直接 Fatal。
	 */
	FString StripJsonAssetName(const FString& Json);

	/** 给资产加一条轨道 */
	UAKAbilityTrack* AddTrack(
		UAKAbilityAsset* Asset,
		EAKTrackType TrackType = EAKTrackType::Effect,
		EAKTrackCompositeMode CompositeMode = EAKTrackCompositeMode::Timeline);

	/** 在轨道上放一个任务（自动设置起止时间） */
	template <class T>
	T* AddTask(UAKAbilityTrack* Track, float StartTime = 0.f, float Duration = 0.f)
	{
		if (!Track)
		{
			return nullptr;
		}

		T* Task = NewObject<T>(Track);
		Task->StartTime = StartTime;
		Task->Duration = Duration;
		Track->Tasks.Add(Task);
		return Task;
	}

	/** 造一个运行实例并初始化（上下文默认权威端 + 本地控制端，任务 NetMode=Both 才会执行） */
	UAKAbilityInstance* NewInstance(UAKAbilityAsset* Asset);

	/** 生成测试载体并设置初始属性 */
	AAKTestGASActor* SpawnGASActor(UWorld* World, float Health, float MaxHealth = 100.f);

	/** 测试产物的根目录：<Project>/Saved/UAKAutomation */
	FString GetAutomationOutputDir();

	/** 本次测试运行的时间戳（YYYYmmdd-HHMMSS），用于归档 */
	FString GetRunStamp();
}
