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

	/** 关闭后该轨道上的任务全部不参与执行（静音 Mute） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	bool bEnabled = true;

	/**
	 * 注意：**这里没有 Solo（独奏）**。
	 *
	 * Solo 是 Sequencer 从多轨媒体（音轨、摄像机轨）带来的概念——"只播放这一条"，
	 * 因为那些轨道天然可以单独听/看。技能不是混音台：一段技能里所有轨道是**协同生效的**，
	 * "只跑这一条"没有对应的运行语义（执行器也不会实现它），硬搬过来只会变成一个
	 * 看着有用、实际什么都不做的装饰字段。调试单条轨道用**静音掉其它轨道**就够了。
	 *
	 * 想临时只看一条：把别的轨道静音（`bEnabled = false`）。
	 */

	/**
	 * 锁定：编辑器里禁改这条轨道（防误改），运行时忽略。
	 * 与 Sequencer 轨道头的 Lock 同一语义：只挡编辑操作，不影响执行。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	bool bLocked = false;

	/** 仅编辑器：折叠显示（折叠后时间轴上只画一条汇总条，不展开任务） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	bool bCollapsed = false;

	/** 这条轨道上的任务是否参与执行：就是 bEnabled（保留函数是为了调用点读起来是"语义"而不是"字段"） */
	bool IsRunning() const { return bEnabled; }

	// ── 复合编排（Sequence / Parallel / Repeat）─────────────────────────────
	/** 该轨道上任务的排布方式 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Composite")
	EAKTrackCompositeMode CompositeMode = EAKTrackCompositeMode::Timeline;

	/** 整条轨道重复执行多少次（仅 Sequence / Parallel 生效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Composite",
		meta = (ClampMin = "1", UIMin = "1", EditCondition = "CompositeMode != EAKTrackCompositeMode::Timeline"))
	int32 RepeatCount = 1;

	/** 每次重复之间的间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Composite",
		meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "CompositeMode != EAKTrackCompositeMode::Timeline"))
	float RepeatInterval = 0.f;

	/** 整条轨道的起始延迟（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Composite", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StartDelay = 0.f;

	/** 轨道上的任务片段（内联实例化） */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Track")
	TArray<TObjectPtr<UAKAbilityTaskBase>> Tasks;

	/** 轨道显示名：未命名时回落到类型名 */
	FText GetDisplayName() const;
};
