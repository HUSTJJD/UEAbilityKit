#include "UAKTargeting.h"

#include "UAKAbilityContext.h"

void UAKTargeting::AppendTargetDataToContext(const FGameplayAbilityTargetDataHandle& Handle, FAKAbilityContext& Context)
{
	for (const TSharedPtr<FGameplayAbilityTargetData>& Data : Handle.Data)
	{
		if (!Data.IsValid())
		{
			continue;
		}

		AppendActorsToContext(Data->GetActors(), Context);
	}
}

void UAKTargeting::AppendActorsToContext(const TArray<TWeakObjectPtr<AActor>>& Actors, FAKAbilityContext& Context)
{
	for (const TWeakObjectPtr<AActor>& Actor : Actors)
	{
		if (Actor.IsValid())
		{
			Context.Targets.AddUnique(Actor);
		}
	}
}
