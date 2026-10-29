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

// 说明：这里刻意**没有** IsEffectivelyEnabled / Solo 之类的东西。
// 技能不是多轨媒体，"只跑这条轨道"没有对应运行语义，与其做个空壳字段，不如不做。

#undef LOCTEXT_NAMESPACE
