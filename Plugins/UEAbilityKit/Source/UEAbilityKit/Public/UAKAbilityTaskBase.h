#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Dom/JsonObject.h"
#include "UAKTypes.h"
#include "UAKAbilityTaskBase.generated.h"

struct FAKAbilityContext;
struct FAKGASImpact;
struct FAKValidationResult;
class UAKAbilityAsset;

/**
 * 时间轴上的一个原子任务（轨道上的片段）。
 *
 * 设计要点：
 * - EditInlineNew + DefaultToInstanced：可直接内联编辑并作为轨道的子对象实例化。
 * - 无状态：同一份资产可被多个实体并行运行，运行时状态一律放进 ScratchPad。
 * - 三个契约：生命周期（执行）/ Validate（自校验）/ GetGASImpact（自述对 GAS 的影响）。
 *   后两者是 UAK 与 Able 的关键差异——Able 的校验是外部绑定的 Validator，
 *   且没有"任务自述 GAS 影响"的概念（它不接入 GAS）。
 */
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class UEABILITYKIT_API UAKAbilityTaskBase : public UObject
{
	GENERATED_BODY()

public:
	/** 在技能时间轴上的起始时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StartTime = 0.f;

	/** 持续时间（秒）。0 表示单帧任务（进入即结束，不参与 Tick） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Duration = 0.f;

	/** 是否需要每帧 Tick（仅对持续型任务有意义） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	bool bNeedsTick = false;

	/** 执行端标记（UAK 不自建同步，这只是"哪一端该执行"的标记） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	EAKTaskNetMode NetMode = EAKTaskNetMode::Both;

	/** 编辑器备注，便于策划之间沟通 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FText Note;

	/**
	 * 即使技能被中断，也保证至少执行一次。
	 *
	 * 典型用途：必中的收尾 GE、必须落地的资源回收、必须发的关键 Cue。
	 * 中断时若该任务还没轮到，执行器会补执行一次（OnTaskStart → OnTaskEnd(false)）；
	 * 若正在运行，则以"未被中断"的方式结束，避免表现突兀。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	bool bAlwaysExecute = false;

	/**
	 * 是否冻结时间轴推进（"门控"）。
	 * 为 true 时，该任务存活期间整个时间轴的时间不前进（但持续任务仍会收到 Tick）。
	 * 用于"等待目标进入范围 / 等待玩家输入"这类语义。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	bool bGateTimeline = false;

	UFUNCTION(BlueprintPure, Category = "Task")
	float GetEndTime() const { return StartTime + Duration; }

	UFUNCTION(BlueprintPure, Category = "Task")
	bool IsSingleFrame() const { return Duration <= 0.f; }

	// ── 契约一：生命周期 ──────────────────────────────────────────────────
	/** 进入任务（单帧任务也走这里） */
	virtual void OnTaskStart(FAKAbilityContext& Context) {}

	/** 持续任务每帧驱动（仅当 bNeedsTick 为 true） */
	virtual void OnTaskTick(FAKAbilityContext& Context, float DeltaTime) {}

	/** 结束任务。bInterrupted 为 true 表示被中断（技能被打断/取消） */
	virtual void OnTaskEnd(FAKAbilityContext& Context, bool bInterrupted) {}

	/**
	 * 是否要求提前结束（早于 EndTime）。
	 * 由 WaitUntil 这类"条件满足即可继续"的任务使用；默认按 EndTime 结束。
	 */
	virtual bool ShouldEndEarly(const FAKAbilityContext& Context) const { return false; }

	/** 创建运行时状态对象；返回 nullptr 表示不需要 ScratchPad */
	virtual UObject* CreateScratchPad(const FAKAbilityContext& Context) { return nullptr; }

	// ── 契约二：自校验 ────────────────────────────────────────────────────
	/** 让错误"无法被配置出来"，而不是事后报错 */
	virtual void Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const;

	// ── 契约三：自述 GAS 影响 ──────────────────────────────────────────────
	/** 供编辑器的「GAS 影响条」与「血缘视图」使用，无需运行游戏 */
	virtual void GetGASImpact(FAKGASImpact& OutImpact) const {}

	// ── 契约四：可读配置（Json 双向）────────────────────────────────────────
	/** 导出自身参数到 Json；子类必须调用 Super，并写入与 ImportJson 对称的字段 */
	virtual void ExportJson(TSharedRef<FJsonObject> OutJson) const;

	/** 从 Json 恢复自身参数；子类必须调用 Super */
	virtual void ImportJson(const FJsonObject& InJson);
};
