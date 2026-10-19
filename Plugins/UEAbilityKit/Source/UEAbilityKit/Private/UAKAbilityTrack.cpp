#include "UAKAbilityTrack.h"

#define LOCTEXT_NAMESPACE "UAKAbilityTrack"

FText UAKAbilityTrack::GetDisplayName() const
{
	if (!TrackName.IsNone())
	{
		return FText::FromName(TrackName);
	}

	static const TMap<EAKTrackType, FText> FallbackNames =
	{
		{ EAKTrackType::Main,			LOCTEXT("Track_Main", "Main") },
		{ EAKTrackType::Effect,			LOCTEXT("Track_Effect", "Effect") },
		{ EAKTrackType::Presentation,	LOCTEXT("Track_Presentation", "Presentation") },
		{ EAKTrackType::Query,			LOCTEXT("Track_Query", "Query") },
		{ EAKTrackType::Motion,			LOCTEXT("Track_Motion", "Motion") },
		{ EAKTrackType::Rule,			LOCTEXT("Track_Rule", "Rule") },
		{ EAKTrackType::Custom,			LOCTEXT("Track_Custom", "Custom") }
	};

	if (const FText* Found = FallbackNames.Find(TrackType))
	{
		return *Found;
	}
	return LOCTEXT("Track_Unknown", "Track");
}

#undef LOCTEXT_NAMESPACE
