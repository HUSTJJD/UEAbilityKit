#include "UAKDemoConditions.h"

#include "UAKAbilityContext.h"
#include "UAKDemoCharacter.h"

#define LOCTEXT_NAMESPACE "UAKDemoConditions"

AAKDemoCharacter* UAKDemoChanneling_InputHeld::GetDemoCharacter(const FAKAbilityContext& Context) const
{
	return Cast<AAKDemoCharacter>(Context.Owner.Get());
}

bool UAKDemoChanneling_InputHeld::IsSatisfied(const FAKAbilityContext& Context) const
{
	const AAKDemoCharacter* Character = GetDemoCharacter(Context);

	// 角色不在（或没接脚本）时按"仍然成立"处理：示例不该因为缺 Lua 就永远打断
	if (!Character)
	{
		return true;
	}

	return Character->IsChannelingInputHeld(SlotIndex);
}

void UAKDemoChanneling_InputHeld::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);
	OutJson->SetNumberField(TEXT("slotIndex"), SlotIndex);
}

void UAKDemoChanneling_InputHeld::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	int32 OutSlot = SlotIndex;
	if (InJson.TryGetNumberField(TEXT("slotIndex"), OutSlot))
	{
		SlotIndex = OutSlot;
	}
}

#if WITH_EDITOR
FText UAKDemoChanneling_InputHeld::GetConditionDescription() const
{
	return FText::Format(LOCTEXT("Desc", "技能槽 {0} 的按键仍被按住"), FText::AsNumber(SlotIndex));
}
#endif

#undef LOCTEXT_NAMESPACE
