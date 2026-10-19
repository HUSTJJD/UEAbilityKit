#include "UAKAbilityTask_Dash.h"

#include "UAKAbilityContext.h"

#include "GameFramework/Character.h"

#define LOCTEXT_NAMESPACE "UAKAbilityTask_Dash"

void UAKAbilityTask_Dash::OnTaskStart(FAKAbilityContext& Context)
{
	ACharacter* Character = Cast<ACharacter>(Context.Owner.Get());
	if (!Character)
	{
		return;
	}

	FVector Direction = LocalDirection.GetSafeNormal();
	if (!bWorldSpace)
	{
		Direction = Character->GetActorRotation().RotateVector(Direction);
	}

	const FVector Velocity = Direction * Strength;
	Character->LaunchCharacter(Velocity, bOverrideHorizontal, bOverrideVertical);
}

void UAKAbilityTask_Dash::GetGASImpact(FAKGASImpact& OutImpact) const
{
	OutImpact.Summary = FText::Format(LOCTEXT("DashSummary", "位移：{0} 速度"), FText::AsNumber(Strength));
}

void UAKAbilityTask_Dash::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);

	TArray<TSharedPtr<FJsonValue>> DirArray;
	DirArray.Add(MakeShared<FJsonValueNumber>(LocalDirection.X));
	DirArray.Add(MakeShared<FJsonValueNumber>(LocalDirection.Y));
	DirArray.Add(MakeShared<FJsonValueNumber>(LocalDirection.Z));
	OutJson->SetArrayField(TEXT("localDirection"), DirArray);

	OutJson->SetNumberField(TEXT("strength"), Strength);
	OutJson->SetBoolField(TEXT("overrideHorizontal"), bOverrideHorizontal);
	OutJson->SetBoolField(TEXT("overrideVertical"), bOverrideVertical);
	OutJson->SetBoolField(TEXT("worldSpace"), bWorldSpace);
}

void UAKAbilityTask_Dash::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	const TArray<TSharedPtr<FJsonValue>>* DirArray = nullptr;
	if (InJson.TryGetArrayField(TEXT("localDirection"), DirArray) && DirArray && DirArray->Num() == 3)
	{
		LocalDirection.X = static_cast<float>((*DirArray)[0]->AsNumber());
		LocalDirection.Y = static_cast<float>((*DirArray)[1]->AsNumber());
		LocalDirection.Z = static_cast<float>((*DirArray)[2]->AsNumber());
	}

	double OutStrength = Strength;
	if (InJson.TryGetNumberField(TEXT("strength"), OutStrength))
	{
		Strength = static_cast<float>(OutStrength);
	}

	InJson.TryGetBoolField(TEXT("overrideHorizontal"), bOverrideHorizontal);
	InJson.TryGetBoolField(TEXT("overrideVertical"), bOverrideVertical);
	InJson.TryGetBoolField(TEXT("worldSpace"), bWorldSpace);
}

#undef LOCTEXT_NAMESPACE
