#include "Misc/AutomationTest.h"

#include "UAKTestCommon.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityInstance.h"
#include "UAKAbilityTrack.h"
#include "UAKChanneling.h"
#include "UAKTrace.h"

#define LOCTEXT_NAMESPACE "UAKTests.Instance"

namespace
{
	int32 IndexOfLog(const FString& SubString)
	{
		const TArray<FString>& Log = UAKAbilityTask_TestProbe::GetLog();
		for (int32 Index = 0; Index < Log.Num(); ++Index)
		{
			if (Log[Index].Contains(SubString))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
}

/**
 * 一次完整施放：任务按时间轴顺序进入并结束，时间轴跑完后实例结束。
 * 这是最基础的"实例测试"——验证执行器真的能把资产推进成一次施放。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKInstanceRunsToEndTest, "UAK.Instance.Timeline.RunsToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKInstanceRunsToEndTest::RunTest(const FString& Parameters)
{
	UAKAbilityTask_TestProbe::ResetLog();

	UAKAbilityAsset* Asset = UAKTest::NewAsset(1.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);

	UAKAbilityTask_TestProbe* First = UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.1f, 0.f);
	First->TaskId = FName(TEXT("First"));

	UAKAbilityTask_TestProbe* Second = UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.5f, 0.f);
	Second->TaskId = FName(TEXT("Second"));

	UAKAbilityInstance* Instance = UAKTest::NewInstance(Asset);
	TestTrue(TEXT("实例应创建成功"), Instance != nullptr);
	if (!Instance)
	{
		return false;
	}

	Instance->Tick(0.2f);   // 0.2s：First 触发
	TestTrue(TEXT("0.2s 时 First 应已开始"), UAKAbilityTask_TestProbe::LogContains(TEXT("START First")));
	TestFalse(TEXT("0.2s 时 Second 还不该开始"), UAKAbilityTask_TestProbe::LogContains(TEXT("START Second")));

	Instance->Tick(0.4f);   // 0.6s：Second 触发
	TestTrue(TEXT("0.6s 时 Second 应已开始"), UAKAbilityTask_TestProbe::LogContains(TEXT("START Second")));

	TestFalse(TEXT("时间轴未跑完时不应结束"), Instance->IsFinished());

	Instance->Tick(0.5f);   // 1.1s：超过资产长度
	TestTrue(TEXT("时间轴跑完后应结束"), Instance->IsFinished());

	// 顺序断言：First 必须先于 Second
	const int32 FirstIndex = IndexOfLog(TEXT("START First"));
	const int32 SecondIndex = IndexOfLog(TEXT("START Second"));
	TestTrue(TEXT("First 应先于 Second 触发"), FirstIndex != INDEX_NONE && SecondIndex != INDEX_NONE && FirstIndex < SecondIndex);

	TestEqual(TEXT("单帧任务应进入即结束"), UAKAbilityTask_TestProbe::CountLog(TEXT("END First")), 1);

	return true;
}

/** 门控任务存活期间，时间轴不前进 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKInstanceGateTest, "UAK.Instance.Gate.FreezesTimeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKInstanceGateTest::RunTest(const FString& Parameters)
{
	UAKAbilityTask_TestProbe::ResetLog();

	UAKAbilityAsset* Asset = UAKTest::NewAsset(5.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);

	UAKAbilityTask_TestProbe* Gate = UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 1.f);
	Gate->TaskId = FName(TEXT("Gate"));
	Gate->bNeedsTick = true;
	Gate->bGateTimeline = true;

	UAKAbilityInstance* Instance = UAKTest::NewInstance(Asset);
	if (!Instance)
	{
		AddError(TEXT("实例创建失败"));
		return false;
	}

	// 第一次推进会前进一个 delta，之后被门控冻住
	for (int32 Index = 0; Index < 5; ++Index)
	{
		Instance->Tick(0.1f);
	}

	TestTrue(TEXT("门控期间时间轴应被冻住"),
		FMath::IsNearlyEqual(Instance->GetContext().CurrentTime, 0.1f, 0.001f));

	TestTrue(TEXT("门控任务本身仍应收到 Tick"), UAKAbilityTask_TestProbe::CountLog(TEXT("TICK Gate")) >= 4);

	return true;
}

/**
 * 中断顺序：默认"后启动的先结束"（栈式回退），
 * 这决定多段技能被打断时表现是否突兀、关键逻辑会不会丢。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKInstanceInterruptOrderTest, "UAK.Instance.Interrupt.ReverseStartOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKInstanceInterruptOrderTest::RunTest(const FString& Parameters)
{
	UAKAbilityTask_TestProbe::ResetLog();

	UAKAbilityAsset* Asset = UAKTest::NewAsset(5.f);
	Asset->InterruptOrder = EAKInterruptOrder::ReverseStartOrder;

	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);

	UAKAbilityTask_TestProbe* Early = UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 1.f);
	Early->TaskId = FName(TEXT("Early"));

	UAKAbilityTask_TestProbe* Late = UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.05f, 1.f);
	Late->TaskId = FName(TEXT("Late"));

	UAKAbilityInstance* Instance = UAKTest::NewInstance(Asset);
	if (!Instance)
	{
		AddError(TEXT("实例创建失败"));
		return false;
	}

	Instance->Tick(0.1f);   // 两个任务都进入运行
	TestEqual(TEXT("两个任务都应已开始"), UAKAbilityTask_TestProbe::CountLog(TEXT("START")), 2);

	Instance->Finish(/*bInterrupted=*/true);

	const int32 LateEnd = IndexOfLog(TEXT("END Late"));
	const int32 EarlyEnd = IndexOfLog(TEXT("END Early"));
	TestTrue(TEXT("后启动的 Late 应先结束"), LateEnd != INDEX_NONE && EarlyEnd != INDEX_NONE && LateEnd < EarlyEnd);
	TestTrue(TEXT("中断时任务应收到 bInterrupted=true"), UAKAbilityTask_TestProbe::LogContains(TEXT("interrupted=1")));

	return true;
}

/** "保证执行"的任务：技能被中断也要补执行一次（必中的收尾 GE、资源回收） */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKInstanceAlwaysExecuteTest, "UAK.Instance.Interrupt.AlwaysExecuteStillRuns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKInstanceAlwaysExecuteTest::RunTest(const FString& Parameters)
{
	UAKAbilityTask_TestProbe::ResetLog();

	UAKAbilityAsset* Asset = UAKTest::NewAsset(5.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);

	UAKAbilityTask_TestProbe* Normal = UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 0.f);
	Normal->TaskId = FName(TEXT("Normal"));

	UAKAbilityTask_TestProbe* Guaranteed = UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.9f, 0.f);
	Guaranteed->TaskId = FName(TEXT("Guaranteed"));
	Guaranteed->bAlwaysExecute = true;

	UAKAbilityInstance* Instance = UAKTest::NewInstance(Asset);
	if (!Instance)
	{
		AddError(TEXT("实例创建失败"));
		return false;
	}

	Instance->Tick(0.1f);   // 只轮到 Normal
	TestFalse(TEXT("中断前 Guaranteed 还没轮到"), UAKAbilityTask_TestProbe::LogContains(TEXT("START Guaranteed")));

	Instance->Finish(/*bInterrupted=*/true);

	TestTrue(TEXT("中断后 Guaranteed 应被补执行"), UAKAbilityTask_TestProbe::LogContains(TEXT("START Guaranteed")));
	TestTrue(TEXT("保证执行的任务不应被标记为中断"),
		UAKAbilityTask_TestProbe::LogContains(TEXT("END Guaranteed")) &&
		IndexOfLog(TEXT("END Guaranteed")) != INDEX_NONE &&
		UAKAbilityTask_TestProbe::GetLog()[IndexOfLog(TEXT("END Guaranteed"))].Contains(TEXT("interrupted=0")));

	return true;
}

/** 循环：跑完一轮后回到 LoopStartTime 再来一遍，MaxLoops 用尽后结束 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKInstanceLoopTest, "UAK.Instance.Loop.Restarts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKInstanceLoopTest::RunTest(const FString& Parameters)
{
	UAKAbilityTask_TestProbe::ResetLog();

	UAKAbilityAsset* Asset = UAKTest::NewAsset(1.f);
	Asset->bLoop = true;
	Asset->MaxLoops = 2;

	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);
	UAKAbilityTask_TestProbe* Probe = UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 0.f);
	Probe->TaskId = FName(TEXT("Tick"));

	UAKAbilityInstance* Instance = UAKTest::NewInstance(Asset);
	if (!Instance)
	{
		AddError(TEXT("实例创建失败"));
		return false;
	}

	Instance->Tick(1.1f);   // 第一轮跑完 → 回到起点
	TestEqual(TEXT("循环计数应为 1"), Instance->GetLoopCount(), 1);
	TestFalse(TEXT("还有循环额度时不应结束"), Instance->IsFinished());

	Instance->Tick(1.1f);   // 第二轮跑完 → MaxLoops 用尽
	TestTrue(TEXT("循环用尽后应结束"), Instance->IsFinished());
	TestEqual(TEXT("任务应被执行 2 次"), UAKAbilityTask_TestProbe::CountLog(TEXT("START Tick")), 2);

	return true;
}

/**
 * 引导：窗口内条件不成立 → 按 CancelAbility 打断。
 * 关键断言：判定发生在任务推进之前，所以"失败的这一帧"不会启动新任务。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKInstanceChannelingTest, "UAK.Instance.Channeling.CancelsOnFailedCondition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKInstanceChannelingTest::RunTest(const FString& Parameters)
{
	UAKAbilityTask_TestProbe::ResetLog();

	UAKAbilityAsset* Asset = UAKTest::NewAsset(2.f);
	Asset->bChanneled = true;
	Asset->ChannelingStartTime = 0.f;
	Asset->ChannelingEndTime = -1.f;    // 到技能结束
	Asset->ChannelFailureAction = EAKChannelFailureAction::CancelAbility;
	Asset->bEvaluateChannelingOnAuthorityOnly = false;

	UAKTestChannelingCondition* Condition = NewObject<UAKTestChannelingCondition>(Asset);
	Condition->bSatisfied = false;
	Asset->ChannelingConditions.Add(Condition);

	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);
	UAKAbilityTask_TestProbe* Probe = UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 0.f);
	Probe->TaskId = FName(TEXT("Channeled"));

	UAKAbilityInstance* Instance = UAKTest::NewInstance(Asset);
	if (!Instance)
	{
		AddError(TEXT("实例创建失败"));
		return false;
	}

	bool bInterrupted = true;
	Instance->OnFinished.BindLambda([&bInterrupted](bool bInInterrupted) { bInterrupted = bInInterrupted; });

	Instance->Tick(0.1f);

	TestTrue(TEXT("条件不成立应立即结束"), Instance->IsFinished());
	TestTrue(TEXT("结束原因应为被打断"), bInterrupted);
	TestFalse(TEXT("失败的这一帧不应启动新任务"), UAKAbilityTask_TestProbe::LogContains(TEXT("START Channeled")));

	// 条件成立时不该打断
	UAKAbilityTask_TestProbe::ResetLog();
	Condition->bSatisfied = true;

	UAKAbilityInstance* OkInstance = UAKTest::NewInstance(Asset);
	OkInstance->Tick(0.1f);
	TestFalse(TEXT("条件成立时不应结束"), OkInstance->IsFinished());
	TestTrue(TEXT("条件成立时任务应正常触发"), UAKAbilityTask_TestProbe::LogContains(TEXT("START Channeled")));

	return true;
}

/** 被 Branch 停用的轨道，其任务不参与本次运行 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKInstanceDisabledTrackTest, "UAK.Instance.Branch.DisabledTrackSkipped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKInstanceDisabledTrackTest::RunTest(const FString& Parameters)
{
	UAKAbilityTask_TestProbe::ResetLog();

	UAKAbilityAsset* Asset = UAKTest::NewAsset(1.f);

	UAKAbilityTrack* MainTrack = UAKTest::AddTrack(Asset, EAKTrackType::Main);
	UAKAbilityTask_TestProbe* MainProbe = UAKTest::AddTask<UAKAbilityTask_TestProbe>(MainTrack, 0.f, 0.f);
	MainProbe->TaskId = FName(TEXT("Main"));

	UAKAbilityTrack* ExtraTrack = UAKTest::AddTrack(Asset, EAKTrackType::Presentation);
	UAKAbilityTask_TestProbe* ExtraProbe = UAKTest::AddTask<UAKAbilityTask_TestProbe>(ExtraTrack, 0.f, 0.f);
	ExtraProbe->TaskId = FName(TEXT("Extra"));

	UAKAbilityInstance* Instance = UAKTest::NewInstance(Asset);
	if (!Instance)
	{
		AddError(TEXT("实例创建失败"));
		return false;
	}

	// 模拟 Branch 任务的判定结果：停用表现轨道
	Instance->GetContext().DisabledTracks.Add(ExtraTrack->TrackName);

	Instance->Tick(0.1f);

	TestTrue(TEXT("启用的轨道应正常执行"), UAKAbilityTask_TestProbe::LogContains(TEXT("START Main")));
	TestFalse(TEXT("被停用的轨道不应执行"), UAKAbilityTask_TestProbe::LogContains(TEXT("START Extra")));

	return true;
}

/** 溯源：开启记录后，任务节点会被写进血缘树（可继续下钻到 GE / 目标 / 属性） */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKInstanceTraceTest, "UAK.Instance.Trace.RecordsTaskNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKInstanceTraceTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(1.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);

	UAKAbilityTask_TestProbe* Probe = UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 0.f);
	Probe->TaskId = FName(TEXT("Traced"));

	UAKTrace* Trace = NewObject<UAKTrace>(GetTransientPackage(), UAKTrace::StaticClass(), NAME_None, RF_Standalone);
	Trace->AddNode(INDEX_NONE, EAKTraceNodeKind::Ability, TEXT("TestAbility"), 0.f);

	UAKAbilityInstance* Instance = UAKTest::NewInstance(Asset);
	if (!Instance)
	{
		AddError(TEXT("实例创建失败"));
		return false;
	}

	Instance->SetTrace(Trace);
	Instance->Tick(0.1f);

	bool bHasTaskNode = false;
	for (const FAKTraceNode& Node : Trace->GetNodes())
	{
		if (Node.Kind == EAKTraceNodeKind::Task && Node.Label == TEXT("Traced"))
		{
			bHasTaskNode = true;
		}
	}

	TestTrue(TEXT("血缘树里应出现该任务节点"), bHasTaskNode);
	TestTrue(TEXT("节点数应大于 1"), Trace->GetNodes().Num() > 1);

	return true;
}

#undef LOCTEXT_NAMESPACE
