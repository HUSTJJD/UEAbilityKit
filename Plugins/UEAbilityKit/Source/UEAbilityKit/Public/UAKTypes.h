#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UAKTypes.generated.h"

class UAKAbilityTaskBase;
class UGameplayEffect;
class UAnimMontage;

/** 轨道类型：决定默认呈现、可接受的任务类别与时间轴绘制方式 */
UENUM(BlueprintType)
enum class EAKTrackType : uint8
{
	Main		UMETA(DisplayName = "Main (时序/动画)"),
	Effect		UMETA(DisplayName = "Effect (逻辑效果)"),
	Presentation UMETA(DisplayName = "Presentation (表现)"),
	Query		UMETA(DisplayName = "Query (目标/碰撞)"),
	Motion		UMETA(DisplayName = "Motion (位移)"),
	Rule		UMETA(DisplayName = "Rule (事件规则)"),
	Custom		UMETA(DisplayName = "Custom")
};

/**
 * 轨道的复合执行模式（AbilityKit Pipeline 的 Sequence/Parallel/Repeat 思想在 UAK 的落地方式）。
 *
 * 设计取舍：AbilityKit 用"Phase 图"表达组合，UAK 把它放进**轨道**这一层——
 * 因为任务本来就是"时间轴上的片段"，组合语义天然属于"这一串片段怎么排"。
 */
UENUM(BlueprintType)
enum class EAKTrackCompositeMode : uint8
{
	/** 按任务自己的绝对时间排布（默认，与手工时间轴一致） */
	Timeline	UMETA(DisplayName = "Timeline (绝对时间)"),
	/** 忽略绝对时间，按数组顺序依次执行：上一个结束，下一个开始 */
	Sequence	UMETA(DisplayName = "Sequence (依次)"),
	/** 忽略绝对时间，全部同时开始 */
	Parallel	UMETA(DisplayName = "Parallel (同时)")
};

/**
 * 中断时结束任务的顺序。
 *
 * 为什么要有这个：多段技能被打断时，"先停哪个"直接决定表现是否突兀、
 * 以及关键逻辑会不会丢。把它显式化，而不是让遍历顺序隐式决定。
 */
UENUM(BlueprintType)
enum class EAKInterruptOrder : uint8
{
	/** 后启动的先结束（栈式撤销，默认）——最符合"回退到打断前"的直觉 */
	ReverseStartOrder	UMETA(DisplayName = "后启动的先结束"),
	/** 按启动顺序结束 */
	StartOrder			UMETA(DisplayName = "按启动顺序"),
	/** 按轨道优先级：先停表现，再停逻辑，最后停主时序 */
	TrackPriority		UMETA(DisplayName = "按轨道优先级")
};

/** 引导 / 吟唱条件失败时做什么 */
UENUM(BlueprintType)
enum class EAKChannelFailureAction : uint8
{
	/** 视为被打断：所有存活任务收到 bInterrupted=true，技能被取消 */
	CancelAbility		UMETA(DisplayName = "取消技能（视为打断）"),
	/** 视为引导完成：任务正常结束，技能正常结束 */
	FinishNormally		UMETA(DisplayName = "正常结束（引导完成）"),
	/** 跳到时间轴末尾再正常结束（跳过剩余编排） */
	SkipToEnd			UMETA(DisplayName = "跳到末尾后结束")
};

/** 执行计划中的一条：任务 + 本次运行的起止时间 + 所属轨道 */
USTRUCT()
struct FAKScheduledTask
{
	GENERATED_BODY()

	UPROPERTY()
	UAKAbilityTaskBase* Task = nullptr;

	UPROPERTY()
	float StartTime = 0.f;

	UPROPERTY()
	float EndTime = 0.f;

	UPROPERTY()
	int32 TrackIndex = INDEX_NONE;
};

/**
 * 任务的执行端标记。
 * 注意：UAK 不自建网络同步——网络交给 GAS（FPredictionKey + ASC 复制）。
 * 这里只是"哪一端应该执行"的标记，用于避免表现类任务在权威端空跑。
 */
UENUM(BlueprintType)
enum class EAKTaskNetMode : uint8
{
	Both		UMETA(DisplayName = "Both"),
	ServerOnly	UMETA(DisplayName = "Server Only"),
	ClientOnly	UMETA(DisplayName = "Client Only (表现)")
};

/** 命名时间点：Windup / Cast / Recover …，供规则与时间轴引用 */
USTRUCT(BlueprintType)
struct FAKTimeMarker
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker")
	FName Name = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Time = 0.f;
};

/**
 * 任务对 GAS 产生的影响。
 * 编辑器据此渲染「GAS 影响条」与「血缘视图」，无需运行游戏。
 */
USTRUCT(BlueprintType)
struct FAKGASImpact
{
	GENERATED_BODY()

	/** 会授予 / 影响的 Tag */
	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	TArray<FGameplayTag> Tags;

	/** 会施加的 GE */
	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	TArray<TSoftClassPtr<UGameplayEffect>> Effects;

	/** 会触发的 GameplayCue Tag */
	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	TArray<FGameplayTag> Cues;

	/** 一句话摘要，供编辑器展示 */
	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	FText Summary;
};

/** 校验严重程度 */
UENUM(BlueprintType)
enum class EAKValidationSeverity : uint8
{
	Info	UMETA(DisplayName = "提示"),
	Warning	UMETA(DisplayName = "警告"),
	Error	UMETA(DisplayName = "错误")
};

/** 一条校验问题 */
USTRUCT(BlueprintType)
struct FAKValidationIssue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Validation")
	EAKValidationSeverity Severity = EAKValidationSeverity::Warning;

	UPROPERTY(BlueprintReadWrite, Category = "Validation")
	FText Message;

	/** 产生问题的任务（可为 null，表示资产级问题） */
	UPROPERTY(BlueprintReadWrite, Category = "Validation")
	UAKAbilityTaskBase* Task = nullptr;
};

/** 校验结果聚合 */
USTRUCT(BlueprintType)
struct FAKValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Validation")
	TArray<FAKValidationIssue> Issues;

	void Add(EAKValidationSeverity Severity, const FText& Message, UAKAbilityTaskBase* InTask = nullptr)
	{
		FAKValidationIssue Issue;
		Issue.Severity = Severity;
		Issue.Message = Message;
		Issue.Task = InTask;
		Issues.Add(MoveTemp(Issue));
	}

	int32 NumErrors() const
	{
		int32 Count = 0;
		for (const FAKValidationIssue& Issue : Issues)
		{
			Count += (Issue.Severity == EAKValidationSeverity::Error) ? 1 : 0;
		}
		return Count;
	}

	int32 NumWarnings() const
	{
		int32 Count = 0;
		for (const FAKValidationIssue& Issue : Issues)
		{
			Count += (Issue.Severity == EAKValidationSeverity::Warning) ? 1 : 0;
		}
		return Count;
	}
};
