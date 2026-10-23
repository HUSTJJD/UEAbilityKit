#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UAKAbilityInstance.h"
#include "UAKDebugSubsystem.generated.h"

class AActor;
class UAKTrace;

/**
 * 调试器用的一行快照。
 *
 * 为什么要快照而不是直接把 UObject 交给 UI：
 * UI 每帧刷新，不应该在渲染线程里做字符串拼接与弱引用判空；
 * 而且快照是值类型，跨模块（Runtime → Editor）传递没有 GC 生命周期问题。
 */
USTRUCT()
struct FAKDebugEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Owner;

	UPROPERTY()
	FString OwnerName;

	/** 技能资产名 */
	UPROPERTY()
	FString AbilityName;

	UPROPERTY()
	float CurrentTime = 0.f;

	UPROPERTY()
	float Length = 0.f;

	UPROPERTY()
	int32 LoopCount = 0;

	/** 时间轴被门控任务冻结（WaitUntil 等待中） */
	UPROPERTY()
	bool bGated = false;

	/** 当前存活任务的可读描述，例如 "PlayMontage (剩 0.42s)" */
	UPROPERTY()
	TArray<FString> ActiveTasks;

	UPROPERTY()
	TWeakObjectPtr<UAKTrace> Trace;
};

/**
 * 运行时调试器（对标 AbleDebugger，但全部走 UAK 自己的 Trace 与实例表）。
 *
 * 三件事：
 * 1. 维护「当前世界里正在跑的技能实例」登记表（UAKGameplayAbility 激活/结束时登记）；
 * 2. 按开关在世界里画浮字（谁在放什么技能、跑到第几秒、卡在哪个门控任务上）；
 * 3. 对外提供快照，供控制台 `UAK.Debug.Summary` 与编辑器面板读取。
 *
 * 关掉时零开销：不注册实例查询、不画、不刷面板。
 */
UCLASS()
class UEABILITYKIT_API UAKDebugSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	/** 全局开关：是否在世界里画标注（`UAK.Debug.Enable` 控制） */
	static bool IsDebugDrawEnabled();
	static void SetDebugDrawEnabled(bool bEnabled);

	/** 实例登记表：由 UAKGameplayAbility 在激活/结束时维护 */
	void RegisterInstance(UAKAbilityInstance* Instance);
	void UnregisterInstance(UAKAbilityInstance* Instance);
	const TArray<TObjectPtr<UAKAbilityInstance>>& GetActiveInstances() const { return ActiveInstances; }

	/** 当前快照（面板与 Summary 命令都用它） */
	TArray<FAKDebugEntry> BuildEntries();

private:
	void DrawWorldOverlay();

	UPROPERTY()
	TArray<TObjectPtr<UAKAbilityInstance>> ActiveInstances;

	/** 快照缓存：复用同一块内存，避免每帧分配 */
	TArray<FAKDebugEntry> CachedEntries;
};
