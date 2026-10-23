#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UAKTrace.generated.h"

/** 溯源节点类型 */
UENUM(BlueprintType)
enum class EAKTraceNodeKind : uint8
{
	Ability		UMETA(DisplayName = "技能"),
	Task		UMETA(DisplayName = "任务"),
	Effect		UMETA(DisplayName = "GE"),
	Cue			UMETA(DisplayName = "Cue"),
	Target		UMETA(DisplayName = "目标"),
	Attribute	UMETA(DisplayName = "属性")
};

/** 一个溯源节点：记录"发生了什么、在什么时间、由谁产生" */
USTRUCT(BlueprintType)
struct FAKTraceNode
{
	GENERATED_BODY()

	/** 父节点索引（INDEX_NONE 表示根节点），由此构成一颗树 */
	UPROPERTY(BlueprintReadOnly, Category = "Trace")
	int32 ParentIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Trace")
	EAKTraceNodeKind Kind = EAKTraceNodeKind::Task;

	UPROPERTY(BlueprintReadOnly, Category = "Trace")
	FString Label;

	UPROPERTY(BlueprintReadOnly, Category = "Trace")
	float Time = 0.f;

	/** 补充信息，例如 "Health -120" / "SetByCaller Damage=120" */
	UPROPERTY(BlueprintReadOnly, Category = "Trace")
	TArray<FString> Details;
};

/**
 * 一次施放的溯源记录（血缘树）。
 *
 * 价值：能反查「这个 120 点伤害是哪来的」——
 * 技能 → 任务 → GE → 目标 → 属性变化，一路可下钻。
 * 这是 Able 不具备的能力（它只能告诉你"现在在跑哪个 Task"）。
 */
UCLASS(BlueprintType)
class UEABILITYKIT_API UAKTrace : public UObject
{
	GENERATED_BODY()

public:
	int32 AddNode(int32 ParentIndex, EAKTraceNodeKind Kind, const FString& Label, float Time);
	void AddDetail(int32 NodeIndex, const FString& Detail);
	void Reset();

	const TArray<FAKTraceNode>& GetNodes() const { return Nodes; }
	int32 GetRootIndex() const { return RootIndex; }

	/** 把血缘树格式化成可读文本（带层级缩进），供控制台命令与调试面板使用 */
	FString ToDebugString() const;

private:
	UPROPERTY()
	TArray<FAKTraceNode> Nodes;

	int32 RootIndex = INDEX_NONE;
};

/** 世界级溯源记录器：编辑器与运行时都从这里取数据 */
UCLASS()
class UEABILITYKIT_API UAKTraceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 是否开启记录（关闭时 BeginTrace 返回 nullptr，零开销） */
	UPROPERTY()
	bool bRecordingEnabled = false;

	UAKTrace* BeginTrace(const FString& Label);
	void EndTrace(UAKTrace* Trace);

	const TArray<TObjectPtr<UAKTrace>>& GetActiveTraces() const { return ActiveTraces; }
	const TArray<TObjectPtr<UAKTrace>>& GetFinishedTraces() const { return FinishedTraces; }

	/** 清空已完成的记录（供 UAK.Trace.Clear 使用；进行中的记录不受影响） */
	void ClearFinished();

private:
	UPROPERTY()
	TArray<TObjectPtr<UAKTrace>> ActiveTraces;

	UPROPERTY()
	TArray<TObjectPtr<UAKTrace>> FinishedTraces;
};
