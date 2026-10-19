#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "Dom/JsonObject.h"
#include "UAKCondition.generated.h"

struct FAKAbilityContext;

/** 数值比较方式 */
UENUM(BlueprintType)
enum class EAKComparison : uint8
{
	GreaterOrEqual	UMETA(DisplayName = "≥"),
	Greater			UMETA(DisplayName = ">"),
	LessOrEqual		UMETA(DisplayName = "≤"),
	Less			UMETA(DisplayName = "<"),
	Equal			UMETA(DisplayName = "==")
};

/**
 * 可复用的条件：全部基于 GAS 原生数据（Tag / 属性），不自研表达式引擎。
 *
 * 被 Branch（分支）与 WaitUntil（等待）共用，保证"条件"在两处语义完全一致。
 */
USTRUCT(BlueprintType)
struct FAKCondition
{
	GENERATED_BODY()

	/** 需要全部满足的 Tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FGameplayTagContainer RequiredTags;

	/** 存在任一即不满足的 Tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FGameplayTagContainer BlockedTags;

	/** 参与比较的属性（留空表示只判断 Tag） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	EAKComparison Comparison = EAKComparison::GreaterOrEqual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	float Threshold = 0.f;

	/** 条件对象：默认取施法者，勾选后取第一个目标 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	bool bEvaluateOnFirstTarget = false;

	/** 取反 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	bool bInvert = false;

	bool Evaluate(const FAKAbilityContext& Context) const;
	bool IsEmpty() const;

	void ExportJson(TSharedRef<FJsonObject> OutJson) const;
	void ImportJson(const FJsonObject& InJson);
};
