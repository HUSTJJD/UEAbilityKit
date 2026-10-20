#include "Misc/AutomationTest.h"

#include "UAKTestCommon.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityJsonSerializer.h"
#include "UAKAbilityTrack.h"
#include "UAKAbilityTask_ApplyGameplayEffect.h"
#include "UAKAbilityTask_AreaEffect.h"
#include "UAKAbilityTask_CollisionQuery.h"
#include "UAKChanneling.h"
#include "UAKQueryFilter.h"

#define LOCTEXT_NAMESPACE "UAKTests.Serialization"

/**
 * Json 往返：导出 → 导入 → 再导出，两次文本必须完全一致。
 *
 * 为什么这条最重要：UAK 的定位是"可读配置"，配置要进 Git、要能被 CI 校验、
 * 要能被引擎外工具改写。一旦往返不稳定（顺序随机、字段丢失、浮点漂移），
 * 这些全部失效。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKSerializationRoundTripTest, "UAK.Unit.Serialization.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKSerializationRoundTripTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(2.f);
	Asset->DisplayName = FText::FromString(TEXT("TestAbility"));
	Asset->Version = 7;

	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset, EAKTrackType::Query, EAKTrackCompositeMode::Timeline);

	UAKAbilityTask_CollisionQuery* Query = UAKTest::AddTask<UAKAbilityTask_CollisionQuery>(Track, 0.1f, 0.f);
	Query->Shape = EAKQueryShape::Cone;
	Query->ConeAngle = 33.f;
	Query->Radius = 250.f;

	UAKQueryFilter_Distance* DistanceFilter = NewObject<UAKQueryFilter_Distance>(Query);
	DistanceFilter->MaxDistance = 321.f;
	Query->Filters.Add(DistanceFilter);

	UAKQueryFilter_Tag* TagFilter = NewObject<UAKQueryFilter_Tag>(Query);
	TagFilter->BlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Dead"))));
	Query->Filters.Add(TagFilter);

	UAKAbilityTask_ApplyGameplayEffect* ApplyEffect =
		UAKTest::AddTask<UAKAbilityTask_ApplyGameplayEffect>(Track, 0.5f, 0.f);
	ApplyEffect->TaskId = FName(TEXT("Damage01"));
	ApplyEffect->SetByCaller.Add(FName(TEXT("Damage")), 120.f);

	Asset->bChanneled = true;
	Asset->ChannelingStartTime = 0.2f;
	Asset->ChannelingEndTime = 1.5f;
	Asset->ChannelFailureAction = EAKChannelFailureAction::SkipToEnd;
	Asset->bEvaluateChannelingOnAuthorityOnly = false;

	UAKChanneling_MovementCondition* MoveCondition = NewObject<UAKChanneling_MovementCondition>(Asset);
	MoveCondition->SpeedThreshold = 42.f;
	Asset->ChannelingConditions.Add(MoveCondition);

	FString JsonA;
	TestTrue(TEXT("ExportToJson 应成功"), UAKAbilityJsonSerializer::ExportToJson(Asset, JsonA));
	TestFalse(TEXT("导出的 Json 不应为空"), JsonA.IsEmpty());

	UAKAbilityAsset* Imported = UAKTest::NewAsset(0.f);
	TestTrue(TEXT("ImportFromJson 应成功"), UAKAbilityJsonSerializer::ImportFromJson(Imported, JsonA));

	FString JsonB;
	TestTrue(TEXT("二次导出应成功"), UAKAbilityJsonSerializer::ExportToJson(Imported, JsonB));

	TestEqual(TEXT("往返后再次导出必须与原文完全一致"), JsonA, JsonB);

	// ── 字段级断言：光看字符串相等不够，还要确认值真的回来了 ──────────────
	TestEqual(TEXT("Version"), Imported->Version, 7);
	TestTrue(TEXT("Length"), FMath::IsNearlyEqual(Imported->Length, 2.f, 0.001f));
	TestEqual(TEXT("轨道数量"), Imported->Tracks.Num(), 1);

	UAKAbilityTrack* ImportedTrack = Imported->Tracks.Num() > 0 ? Imported->Tracks[0] : nullptr;
	TestTrue(TEXT("轨道存在"), ImportedTrack != nullptr);

	if (ImportedTrack)
	{
		TestEqual(TEXT("轨道上的任务数量"), ImportedTrack->Tasks.Num(), 2);

		UAKAbilityTask_CollisionQuery* ImportedQuery = Cast<UAKAbilityTask_CollisionQuery>(
			ImportedTrack->Tasks.Num() > 0 ? ImportedTrack->Tasks[0] : nullptr);
		TestTrue(TEXT("第一个任务应是 CollisionQuery"), ImportedQuery != nullptr);

		if (ImportedQuery)
		{
			TestTrue(TEXT("查询形状 = Cone"), ImportedQuery->Shape == EAKQueryShape::Cone);
			TestTrue(TEXT("锥形角度"), FMath::IsNearlyEqual(ImportedQuery->ConeAngle, 33.f, 0.001f));
			TestEqual(TEXT("过滤器数量"), ImportedQuery->Filters.Num(), 2);

			UAKQueryFilter_Distance* ImportedDistance = Cast<UAKQueryFilter_Distance>(
				ImportedQuery->Filters.Num() > 0 ? ImportedQuery->Filters[0] : nullptr);
			TestTrue(TEXT("第一个过滤器应是距离过滤"), ImportedDistance != nullptr);
			if (ImportedDistance)
			{
				TestTrue(TEXT("距离阈值"), FMath::IsNearlyEqual(ImportedDistance->MaxDistance, 321.f, 0.001f));
			}
		}
	}

	TestTrue(TEXT("bChanneled"), Imported->bChanneled);
	TestEqual(TEXT("引导条件数量"), Imported->ChannelingConditions.Num(), 1);
	TestTrue(TEXT("引导失败动作 = SkipToEnd"),
		Imported->ChannelFailureAction == EAKChannelFailureAction::SkipToEnd);

	UAKChanneling_MovementCondition* ImportedCondition = Cast<UAKChanneling_MovementCondition>(
		Imported->ChannelingConditions.Num() > 0 ? Imported->ChannelingConditions[0] : nullptr);
	TestTrue(TEXT("引导条件类型应为移动条件"), ImportedCondition != nullptr);
	if (ImportedCondition)
	{
		TestTrue(TEXT("速度阈值"), FMath::IsNearlyEqual(ImportedCondition->SpeedThreshold, 42.f, 0.001f));
	}

	return true;
}

/**
 * 稳定标识符 TaskId 必须能往返。
 *
 * 没有它的后果：血缘树里只能看到"ApplyGE"，无法回答"是哪个 ApplyGE 打的这 120 伤害"。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKSerializationTaskIdTest, "UAK.Unit.Serialization.TaskIdPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKSerializationTaskIdTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(1.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);

	UAKAbilityTask_TestProbe* Probe = UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 0.f);
	Probe->TaskId = FName(TEXT("WindupProbe"));

	FString Json;
	TestTrue(TEXT("导出"), UAKAbilityJsonSerializer::ExportToJson(Asset, Json));

	UAKAbilityAsset* Imported = UAKTest::NewAsset(0.f);
	TestTrue(TEXT("导入"), UAKAbilityJsonSerializer::ImportFromJson(Imported, Json));

	UAKAbilityTaskBase* ImportedTask = (Imported->Tracks.Num() > 0 && Imported->Tracks[0]
		&& Imported->Tracks[0]->Tasks.Num() > 0) ? Imported->Tracks[0]->Tasks[0] : nullptr;

	TestTrue(TEXT("任务应被重建"), ImportedTask != nullptr);
	if (ImportedTask)
	{
		TestEqual(TEXT("TaskId 必须被保留"), ImportedTask->TaskId.ToString(), FString(TEXT("WindupProbe")));
	}

	return true;
}

/**
 * 轨道的复合编排参数（模式 / 重复次数 / 间隔 / 起始延迟）必须能往返。
 * 这几个字段是"复合轨道"的全部配置，漏一个就等于编排丢失。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKSerializationTrackCompositeTest, "UAK.Unit.Serialization.TrackCompositePreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKSerializationTrackCompositeTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(3.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset, EAKTrackType::Effect, EAKTrackCompositeMode::Sequence);
	Track->RepeatCount = 3;
	Track->RepeatInterval = 0.25f;
	Track->StartDelay = 0.5f;
	UAKTest::AddTask<UAKAbilityTask_TestProbe>(Track, 0.f, 0.2f);

	FString Json;
	TestTrue(TEXT("导出"), UAKAbilityJsonSerializer::ExportToJson(Asset, Json));

	UAKAbilityAsset* Imported = UAKTest::NewAsset(0.f);
	TestTrue(TEXT("导入"), UAKAbilityJsonSerializer::ImportFromJson(Imported, Json));

	UAKAbilityTrack* ImportedTrack = Imported->Tracks.Num() > 0 ? Imported->Tracks[0] : nullptr;
	TestTrue(TEXT("轨道应被重建"), ImportedTrack != nullptr);

	if (ImportedTrack)
	{
		TestTrue(TEXT("复合模式 = Sequence"), ImportedTrack->CompositeMode == EAKTrackCompositeMode::Sequence);

		TestEqual(TEXT("重复次数"), ImportedTrack->RepeatCount, 3);
		TestTrue(TEXT("重复间隔"), FMath::IsNearlyEqual(ImportedTrack->RepeatInterval, 0.25f, 0.001f));
		TestTrue(TEXT("起始延迟"), FMath::IsNearlyEqual(ImportedTrack->StartDelay, 0.5f, 0.001f));
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
