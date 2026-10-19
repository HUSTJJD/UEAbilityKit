#include "SUAKGASImpactBar.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"
#include "UAKTypes.h"

#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SUAKGASImpactBar"

void SUAKGASImpactBar::Construct(const FArguments& InArgs)
{
	Asset = InArgs._Asset;
	CursorTime = InArgs._CursorTime;

	SetCanTick(true);

	ChildSlot
	[
		SAssignNew(SummaryText, STextBlock)
		.Text(this, &SUAKGASImpactBar::GetSummaryText)
	];
}

void SUAKGASImpactBar::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const float Time = CursorTime.Get(0.f);
	if (!FMath::IsNearlyEqual(Time, LastTime, 0.001f))
	{
		LastTime = Time;
		Recompute();
	}
}

void SUAKGASImpactBar::Recompute()
{
	EffectCount = 0;
	CueCount = 0;
	TagCount = 0;

	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		return;
	}

	const float Time = CursorTime.Get(0.f);

	for (const UAKAbilityTrack* Track : AssetPtr->Tracks)
	{
		if (!Track || !Track->bEnabled)
		{
			continue;
		}

		for (const UAKAbilityTaskBase* Task : Track->Tasks)
		{
			if (!Task)
			{
				continue;
			}

			// "此刻生效"：时间区间覆盖当前光标
			const bool bActive = (Time >= Task->StartTime) && (Time <= Task->GetEndTime());
			if (!bActive)
			{
				continue;
			}

			FAKGASImpact Impact;
			Task->GetGASImpact(Impact);
			EffectCount += Impact.Effects.Num();
			CueCount += Impact.Cues.Num();
			TagCount += Impact.Tags.Num();
		}
	}
}

FText SUAKGASImpactBar::GetSummaryText() const
{
	return FText::Format(LOCTEXT("ImpactSummary", "GE {0} · Cue {1} · Tags {2}"),
		FText::AsNumber(EffectCount), FText::AsNumber(CueCount), FText::AsNumber(TagCount));
}

#undef LOCTEXT_NAMESPACE
