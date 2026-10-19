#include "UAKAbilityTaskBase.h"

#include "UAKAbilityAsset.h"

#define LOCTEXT_NAMESPACE "UAKAbilityTaskBase"

void UAKAbilityTaskBase::Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const
{
	// 通用规则：时间区间必须落在时间轴内
	if (Asset && GetEndTime() > Asset->Length + KINDA_SMALL_NUMBER)
	{
		OutResult.Add(EAKValidationSeverity::Warning,
			FText::Format(LOCTEXT("TaskOverflow", "任务结束时间 {0}s 超出了技能长度 {1}s。"),
				FText::AsNumber(GetEndTime()), FText::AsNumber(Asset->Length)),
			const_cast<UAKAbilityTaskBase*>(this));
	}

	// 通用规则：声明了 Tick 却是单帧任务，属于配置矛盾
	if (bNeedsTick && IsSingleFrame())
	{
		OutResult.Add(EAKValidationSeverity::Warning,
			LOCTEXT("TickOnSingleFrame", "该任务勾选了 NeedsTick，但持续时间为 0（单帧任务不会收到 Tick）。"),
			const_cast<UAKAbilityTaskBase*>(this));
	}
}

void UAKAbilityTaskBase::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	OutJson->SetStringField(TEXT("$type"), GetClass()->GetName());
	OutJson->SetNumberField(TEXT("startTime"), StartTime);
	OutJson->SetNumberField(TEXT("duration"), Duration);
	OutJson->SetBoolField(TEXT("needsTick"), bNeedsTick);
	OutJson->SetStringField(TEXT("netMode"), StaticEnum<EAKTaskNetMode>()->GetNameStringByValue(static_cast<int64>(NetMode)));

	if (!Note.IsEmpty())
	{
		OutJson->SetStringField(TEXT("note"), Note.ToString());
	}
}

void UAKAbilityTaskBase::ImportJson(const FJsonObject& InJson)
{
	double OutStart = StartTime;
	if (InJson.TryGetNumberField(TEXT("startTime"), OutStart))
	{
		StartTime = static_cast<float>(OutStart);
	}

	double OutDuration = Duration;
	if (InJson.TryGetNumberField(TEXT("duration"), OutDuration))
	{
		Duration = static_cast<float>(OutDuration);
	}

	bool bOutTick = bNeedsTick;
	if (InJson.TryGetBoolField(TEXT("needsTick"), bOutTick))
	{
		bNeedsTick = bOutTick;
	}

	FString NetModeName;
	if (InJson.TryGetStringField(TEXT("netMode"), NetModeName))
	{
		if (const UEnum* Enum = StaticEnum<EAKTaskNetMode>())
		{
			NetMode = static_cast<EAKTaskNetMode>(Enum->GetValueByNameString(NetModeName));
		}
	}

	FString NoteText;
	if (InJson.TryGetStringField(TEXT("note"), NoteText))
	{
		Note = FText::FromString(NoteText);
	}
}

#undef LOCTEXT_NAMESPACE
