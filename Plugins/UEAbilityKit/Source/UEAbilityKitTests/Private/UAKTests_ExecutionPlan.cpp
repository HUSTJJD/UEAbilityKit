#include "Misc/AutomationTest.h"

#include "UAKTestCommon.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTrack.h"

#define LOCTEXT_NAMESPACE "UAKTests.ExecutionPlan"

namespace
{
	bool NearlyEqual(float A, float B) { return FMath::IsNearlyEqual(A, B, 0.0001f); }
}

/** Timeline：任务用自己的绝对时间，计划时间应与之一致 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKPlanTimelineTest, "UAK.Unit.Plan.TimelineUsesAbsoluteTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKPlanTimelineTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(2.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset, EAKTrackType::Effect, EAKTrackCompositeMode::Timeline);
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.1f, 0.2f);
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.5f, 0.2f);

	TArray<FAKScheduledTask> Plan;
	Asset->BuildExecutionPlan(Plan);

	TestEqual(TEXT("计划条目数"), Plan.Num(), 2);
	if (Plan.Num() == 2)
	{
		TestTrue(TEXT("第一个任务起点 = 0.1"), NearlyEqual(Plan[0].StartTime, 0.1f));
		TestTrue(TEXT("第二个任务起点 = 0.5"), NearlyEqual(Plan[1].StartTime, 0.5f));
		TestTrue(TEXT("第一个任务终点 = 0.3"), NearlyEqual(Plan[0].EndTime, 0.3f));
	}

	return true;
}

/** Sequence：忽略绝对时间，上一个结束下一个开始 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKPlanSequenceTest, "UAK.Unit.Plan.SequenceChainsTasks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKPlanSequenceTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(5.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset, EAKTrackType::Effect, EAKTrackCompositeMode::Sequence);
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, /*StartTime=*/99.f, 0.2f);  // 绝对时间应被忽略
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, /*StartTime=*/99.f, 0.3f);

	TArray<FAKScheduledTask> Plan;
	Asset->BuildExecutionPlan(Plan);

	TestEqual(TEXT("计划条目数"), Plan.Num(), 2);
	if (Plan.Num() == 2)
	{
		TestTrue(TEXT("第一个从 0 开始"), NearlyEqual(Plan[0].StartTime, 0.f));
		TestTrue(TEXT("第二个接在第一个之后（0.2）"), NearlyEqual(Plan[1].StartTime, 0.2f));
		TestTrue(TEXT("第二个终点 = 0.5"), NearlyEqual(Plan[1].EndTime, 0.5f));
	}

	return true;
}

/** Parallel：全部同时开始 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKPlanParallelTest, "UAK.Unit.Plan.ParallelStartsTogether",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKPlanParallelTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(5.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset, EAKTrackType::Effect, EAKTrackCompositeMode::Parallel);
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 0.2f);
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 0.4f);

	TArray<FAKScheduledTask> Plan;
	Asset->BuildExecutionPlan(Plan);

	TestEqual(TEXT("计划条目数"), Plan.Num(), 2);
	if (Plan.Num() == 2)
	{
		TestTrue(TEXT("两个任务同时开始"), NearlyEqual(Plan[0].StartTime, Plan[1].StartTime));

		// 两条同时开始 → 计划里的先后顺序没有保证，断言"两个终点都存在"而不是"第 1 条是 0.4"
		const bool bHasShort = NearlyEqual(Plan[0].EndTime, 0.2f) || NearlyEqual(Plan[1].EndTime, 0.2f);
		const bool bHasLong = NearlyEqual(Plan[0].EndTime, 0.4f) || NearlyEqual(Plan[1].EndTime, 0.4f);
		TestTrue(TEXT("终点各自按自己的时长（0.2 与 0.4 都应在计划里）"), bHasShort && bHasLong);
	}

	return true;
}

/** Repeat：整条轨道重复 N 遍，并计入间隔 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKPlanRepeatTest, "UAK.Unit.Plan.RepeatExpandsTrack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKPlanRepeatTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(10.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset, EAKTrackType::Effect, EAKTrackCompositeMode::Sequence);
	Track->RepeatCount = 2;
	Track->RepeatInterval = 0.5f;
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 0.2f);
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 0.3f);   // 一轮 = 0.5s

	TArray<FAKScheduledTask> Plan;
	Asset->BuildExecutionPlan(Plan);

	TestEqual(TEXT("2 次重复 × 2 个任务 = 4 条"), Plan.Num(), 4);
	if (Plan.Num() == 4)
	{
		// 一轮 0.5s + 间隔 0.5s = 第二轮从 1.0 开始
		TestTrue(TEXT("第二轮起点 = 1.0"), NearlyEqual(Plan[2].StartTime, 1.0f));
		TestTrue(TEXT("第二轮第二个任务 = 1.2"), NearlyEqual(Plan[3].StartTime, 1.2f));
	}

	return true;
}

/** StartDelay：整条轨道整体后移 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKPlanStartDelayTest, "UAK.Unit.Plan.StartDelayShiftsTrack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKPlanStartDelayTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(5.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset, EAKTrackType::Effect, EAKTrackCompositeMode::Timeline);
	Track->StartDelay = 0.5f;
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.1f, 0.2f);

	TArray<FAKScheduledTask> Plan;
	Asset->BuildExecutionPlan(Plan);

	TestEqual(TEXT("计划条目数"), Plan.Num(), 1);
	if (Plan.Num() == 1)
	{
		TestTrue(TEXT("起点 = 0.1 + 0.5"), NearlyEqual(Plan[0].StartTime, 0.6f));
		TestTrue(TEXT("终点 = 0.3 + 0.5"), NearlyEqual(Plan[0].EndTime, 0.8f));
	}

	return true;
}

/** 被静音的轨道不参与执行 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKPlanDisabledTrackTest, "UAK.Unit.Plan.DisabledTrackIsSkipped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKPlanDisabledTrackTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(2.f);
	UAKAbilityTrack* TrackA = UAKTest::AddTrack(Asset);
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(TrackA, 0.f, 0.1f);

	UAKAbilityTrack* TrackB = UAKTest::AddTrack(Asset, EAKTrackType::Presentation);
	TrackB->bEnabled = false;
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(TrackB, 0.f, 0.1f);

	TArray<FAKScheduledTask> Plan;
	Asset->BuildExecutionPlan(Plan, /*bOnlyEnabledTracks=*/true);
	TestEqual(TEXT("默认跳过静音轨道"), Plan.Num(), 1);

	TArray<FAKScheduledTask> PlanAll;
	Asset->BuildExecutionPlan(PlanAll, /*bOnlyEnabledTracks=*/false);
	TestEqual(TEXT("显式包含全部轨道"), PlanAll.Num(), 2);

	return true;
}

#undef LOCTEXT_NAMESPACE
