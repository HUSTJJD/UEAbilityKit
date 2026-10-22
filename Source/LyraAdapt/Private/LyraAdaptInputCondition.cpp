#include "LyraAdaptInputCondition.h"

ULyraAdaptInputCondition::ULyraAdaptInputCondition()
{
	// 松手是客户端先感知的，必须通知服务器，否则服务器上技能还在跑
	bRequiresServerNotification = true;
}

void ULyraAdaptInputCondition::SetHeld(bool bInHeld)
{
	bHeld = bInHeld;
}

bool ULyraAdaptInputCondition::IsSatisfied(const FAKAbilityContext& Context) const
{
	return bHeld;
}
