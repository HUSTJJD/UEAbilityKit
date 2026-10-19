#include "UAKCondition.h"

#include "UAKAbilityContext.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

bool FAKCondition::IsEmpty() const
{
	return RequiredTags.Num() == 0
		&& BlockedTags.Num() == 0
		&& !Attribute.IsValid();
}

bool FAKCondition::Evaluate(const FAKAbilityContext& Context) const
{
	UAbilitySystemComponent* ASC = Context.SourceASC.Get();
	AActor* ProbeActor = (bEvaluateOnFirstTarget && Context.Targets.Num() > 0)
		? Context.Targets[0].Get()
		: Context.Owner.Get();

	if (!ASC || !ProbeActor)
	{
		return false;
	}

	// Tag 条件
	if (RequiredTags.Num() > 0 && !ASC->HasAllMatchingGameplayTags(RequiredTags))
	{
		return false;
	}
	if (BlockedTags.Num() > 0 && ASC->HasAnyMatchingGameplayTags(BlockedTags))
	{
		return false;
	}

	// 属性条件
	if (Attribute.IsValid())
	{
		bool bFound = false;
		const float Value = UAbilitySystemBlueprintLibrary::GetFloatAttribute(ProbeActor, Attribute, bFound);
		if (!bFound)
		{
			return false;
		}

		bool bPass = false;
		switch (Comparison)
		{
		case EAKComparison::GreaterOrEqual:	bPass = (Value >= Threshold); break;
		case EAKComparison::Greater:		bPass = (Value > Threshold); break;
		case EAKComparison::LessOrEqual:	bPass = (Value <= Threshold); break;
		case EAKComparison::Less:			bPass = (Value < Threshold); break;
		case EAKComparison::Equal:
		default:							bPass = FMath::IsNearlyEqual(Value, Threshold); break;
		}

		if (!bPass)
		{
			return false;
		}
	}

	return bInvert ? false : true;
}

void FAKCondition::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	OutJson->SetStringField(TEXT("requiredTags"), RequiredTags.ToString());
	OutJson->SetStringField(TEXT("blockedTags"), BlockedTags.ToString());
	OutJson->SetStringField(TEXT("attribute"), Attribute.GetName());
	OutJson->SetStringField(TEXT("comparison"),
		StaticEnum<EAKComparison>()->GetNameStringByValue(static_cast<int64>(Comparison)));
	OutJson->SetNumberField(TEXT("threshold"), Threshold);
	OutJson->SetBoolField(TEXT("evaluateOnFirstTarget"), bEvaluateOnFirstTarget);
	OutJson->SetBoolField(TEXT("invert"), bInvert);
}

void FAKCondition::ImportJson(const FJsonObject& InJson)
{
	auto ImportTags = [&InJson](const FString& Field, FGameplayTagContainer& OutTags)
	{
		FString TagsString;
		if (InJson.TryGetStringField(Field, TagsString) && !TagsString.IsEmpty())
		{
			OutTags.Reset();
			OutTags.FromExportString(TagsString);
		}
	};
	ImportTags(TEXT("requiredTags"), RequiredTags);
	ImportTags(TEXT("blockedTags"), BlockedTags);

	// FGameplayAttribute 由 UProperty 指针构成，无法从字符串安全重建，
	// 因此导入时不恢复属性条件（Tag 条件与阈值仍会恢复），由校验器提示。

	FString ComparisonName;
	if (InJson.TryGetStringField(TEXT("comparison"), ComparisonName))
	{
		if (const UEnum* Enum = StaticEnum<EAKComparison>())
		{
			Comparison = static_cast<EAKComparison>(Enum->GetValueByNameString(ComparisonName));
		}
	}

	double OutThreshold = Threshold;
	if (InJson.TryGetNumberField(TEXT("threshold"), OutThreshold))
	{
		Threshold = static_cast<float>(OutThreshold);
	}

	InJson.TryGetBoolField(TEXT("evaluateOnFirstTarget"), bEvaluateOnFirstTarget);
	InJson.TryGetBoolField(TEXT("invert"), bInvert);
}
