#include "Misc/AutomationTest.h"

#include "UAKTestCommon.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTrack.h"
#include "UAKAbilityTask_ApplyGameplayEffect.h"
#include "UAKChanneling.h"

#define LOCTEXT_NAMESPACE "UAKTests.Validation"

/** 一条"干净"的资产不应产生错误（可以有提示级问题） */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKValidationCleanAssetTest, "UAK.Unit.Validation.CleanAssetHasNoError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKValidationCleanAssetTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(1.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 0.5f);

	FAKValidationResult Result;
	Asset->Validate(Result);

	TestEqual(TEXT("干净资产的错误数应为 0"), Result.NumErrors(), 0);
	return true;
}

/** 长度非法的资产必须报错（错误应该"无法被配置出来"） */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKValidationInvalidLengthTest, "UAK.Unit.Validation.InvalidLengthIsError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKValidationInvalidLengthTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(0.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 0.f);

	FAKValidationResult Result;
	Asset->Validate(Result);

	TestTrue(TEXT("长度为 0 应产生至少一条错误"), Result.NumErrors() >= 1);
	return true;
}

/** 任务超出时间轴长度必须被校验拦住 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKValidationTaskOverflowTest, "UAK.Unit.Validation.TaskOverflowIsReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKValidationTaskOverflowTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(1.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);

	// 起点 0.8 + 时长 0.6 = 1.4，超出资产长度 1.0
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.8f, 0.6f);

	FAKValidationResult Result;
	Asset->Validate(Result);

	TestTrue(TEXT("超出时间轴的任务应被报告（错误或警告）"), Result.Issues.Num() >= 1);
	return true;
}

/** ApplyGameplayEffect 没有指定 GE 时必须报错 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKValidationMissingEffectTest, "UAK.Unit.Validation.MissingEffectClassIsError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKValidationMissingEffectTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(1.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);
	UAKTest::AddTask<UAKAbilityTask_ApplyGameplayEffect>(Track, 0.f, 0.f);

	FAKValidationResult Result;
	Asset->Validate(Result);

	TestTrue(TEXT("未指定 GE 应产生错误"), Result.NumErrors() >= 1);
	return true;
}

/** 引导窗口起点晚于终点必须报错 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKValidationChannelWindowTest, "UAK.Unit.Validation.ChannelingWindowIsChecked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKValidationChannelWindowTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(2.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 0.5f);

	Asset->bChanneled = true;
	Asset->ChannelingStartTime = 1.5f;
	Asset->ChannelingEndTime = 0.5f;   // 起点晚于终点

	FAKValidationResult BadResult;
	Asset->Validate(BadResult);
	TestTrue(TEXT("起点晚于终点应报错"), BadResult.NumErrors() >= 1);

	// 修好窗口，但没有条件 → 至少一条警告
	Asset->ChannelingStartTime = 0.f;
	Asset->ChannelingEndTime = 1.f;

	FAKValidationResult NoConditionResult;
	Asset->Validate(NoConditionResult);
	TestTrue(TEXT("引导但无条件应警告"), NoConditionResult.NumWarnings() >= 1);

	// 补上条件 → 干净
	UAKTestChannelingCondition* Condition = NewObject<UAKTestChannelingCondition>(Asset);
	Asset->ChannelingConditions.Add(Condition);

	FAKValidationResult GoodResult;
	Asset->Validate(GoodResult);
	TestEqual(TEXT("补齐条件后不应再有错误"), GoodResult.NumErrors(), 0);

	return true;
}

#undef LOCTEXT_NAMESPACE
