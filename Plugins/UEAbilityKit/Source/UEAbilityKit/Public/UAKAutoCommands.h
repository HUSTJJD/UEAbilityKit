#pragma once

#include "CoreMinimal.h"

/**
 * 自动化桥接命令：UAK.Auto.*
 *
 * 存在的理由：Editor 类型的模块（UEAbilityKitTests）在 -game / -server 进程里不一定加载，
 * 所以"进程外 e2e"（独立游戏、DS + 多客户端）不能靠 Automation 用例驱动，
 * 只能靠**任何进程都认识的东西**——控制台命令。
 *
 * 这些命令跑在 Runtime 模块里，因此编辑器 / Standalone Game / Dedicated Server 都能用：
 *
 *   UAK.Auto.ActivateAbility <ClassPath> [Period]  在（本地玩家的）ASC 上激活技能；Period>0 时周期重试
 *   UAK.Auto.Snapshot <Role> [Period]         把当前 GAS 状态写成 JSON（Period>0 时按秒重复写）
 *   UAK.Auto.Record <Name> [Frames] [Fps]     逐帧截图 → Saved/UAKAutomation/<Name>/frames/
 *   UAK.Auto.Screenshot <Name>                单张截图 → Saved/UAKAutomation/<Name>/shot.png
 *
 * 每条命令都会打一行机器可读的标记，供 Tools/test/report/generate_report.py 汇总：
 *   UAK_E2E <Action> <Name> <OK|FAIL|SKIP> [k=v ...]
 */
class UEABILITYKIT_API FUAKAutoCommands
{
public:
	static void Register();
	static void Unregister();

	/** 素材根目录：<Project>/Saved/UAKAutomation */
	static FString GetMediaRoot();
};
