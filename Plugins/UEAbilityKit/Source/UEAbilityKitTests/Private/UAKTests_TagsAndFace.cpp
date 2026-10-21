#include "Misc/AutomationTest.h"

#include "UAKTestCommon.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityInstance.h"
#include "UAKAbilityJsonSerializer.h"
#include "UAKAbilityTrack.h"
#include "UAKAbilityTask_ApplyLooseTags.h"
#include "UAKAbilityTask_FaceTarget.h"
#include "UAKTypes.h"

#include "UEAbilityKitModule.h"

#include "AbilitySystemComponent.h"

#define LOCTEXT_NAMESPACE "UAKTests.TagsAndFace"

// 单测需要一个真实的 Tag：取运行时模块注册的原生 Tag（见 UEAbilityKitModule.cpp）。
// 不能定义在测试模块里——它的模块类型是 Editor，引擎会拒绝注册原生 Tag。
FGameplayTag TestLooseTag() { return GetUAKTestTag(); }



/**
 * 新增内置任务的 Json 往返。
 *
 * 原则：每加一个任务就要补一条往返用例——
 * 否则"配置可 diff"这个卖点会随任务变多而悄悄失效（字段漏了没人知道）。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKNewTasksJsonRoundTripTest, "UAK.Unit.Task.NewTasks.JsonRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKNewTasksJsonRoundTripTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(2.f);

	UAKAbilityTrack* TagsTrack = UAKTest::AddTrack(Asset, EAKTrackType::Effect);
	UAKAbilityTask_ApplyLooseTags* TagsTask = UAKTest::AddTask<UAKAbilityTask_ApplyLooseTags>(TagsTrack, 0.f, 0.8f);
	TagsTask->TaskId = FName(TEXT("SuperArmor"));
	TagsTask->Tags.AddTag(TestLooseTag());
	TagsTask->bReplicate = false;
	TagsTask->bRemoveOnInterrupt = false;

	UAKAbilityTrack* MotionTrack = UAKTest::AddTrack(Asset, EAKTrackType::Motion);
	UAKAbilityTask_FaceTarget* FaceTask = UAKTest::AddTask<UAKAbilityTask_FaceTarget>(MotionTrack, 0.1f, 0.3f);
	FaceTask->TaskId = FName(TEXT("Turn"));
	FaceTask->Mode = EAKFaceMode::WorldDirection;
	FaceTask->WorldDirection = FVector(0.f, -1.f, 0.f);
	FaceTask->bInstant = false;
	FaceTask->bNeedsTick = true;
	FaceTask->TurnSpeed = 540.f;
	FaceTask->bOnlyYaw = false;
	FaceTask->bCancelWhenNoTarget = true;

	FString JsonA;
	TestTrue(TEXT("导出应成功"), UAKAbilityJsonSerializer::ExportToJson(Asset, JsonA));

	UAKAbilityAsset* Imported = UAKTest::NewAsset(0.f);
	TestTrue(TEXT("导入应成功"), UAKAbilityJsonSerializer::ImportFromJson(Imported, JsonA));

	FString JsonB;
	TestTrue(TEXT("二次导出应成功"), UAKAbilityJsonSerializer::ExportToJson(Imported, JsonB));
	TestEqual(TEXT("往返后再次导出必须与原文完全一致（资产名除外）"),
		UAKTest::StripJsonAssetName(JsonA), UAKTest::StripJsonAssetName(JsonB));

	// ── 字段级断言 ────────────────────────────────────────────────────
	TestEqual(TEXT("应有两条轨道"), Imported->Tracks.Num(), 2);

	if (Imported->Tracks.Num() == 2)
	{
		UAKAbilityTask_ApplyLooseTags* ImportedTags =
			Cast<UAKAbilityTask_ApplyLooseTags>(Imported->Tracks[0]->Tasks.Num() > 0 ? Imported->Tracks[0]->Tasks[0] : nullptr);
		TestTrue(TEXT("第一个任务应是 ApplyLooseTags"), ImportedTags != nullptr);

		if (ImportedTags)
		{
			TestEqual(TEXT("Tag 数量"), ImportedTags->Tags.Num(), 1);
			TestTrue(TEXT("Tag 应还原"), ImportedTags->Tags.HasTag(TestLooseTag()));
			TestFalse(TEXT("bReplicate 应还原为 false"), ImportedTags->bReplicate);
			TestFalse(TEXT("bRemoveOnInterrupt 应还原为 false"), ImportedTags->bRemoveOnInterrupt);
		}

		UAKAbilityTask_FaceTarget* ImportedFace =
			Cast<UAKAbilityTask_FaceTarget>(Imported->Tracks[1]->Tasks.Num() > 0 ? Imported->Tracks[1]->Tasks[0] : nullptr);
		TestTrue(TEXT("第二个任务应是 FaceTarget"), ImportedFace != nullptr);

		if (ImportedFace)
		{
			TestEqual(TEXT("转向模式"), static_cast<uint8>(ImportedFace->Mode), static_cast<uint8>(EAKFaceMode::WorldDirection));
			TestTrue(TEXT("世界方向应还原"), ImportedFace->WorldDirection.Equals(FVector(0.f, -1.f, 0.f)));
			TestFalse(TEXT("应为平滑转向"), ImportedFace->bInstant);
			TestTrue(TEXT("角速度应还原"), FMath::IsNearlyEqual(ImportedFace->TurnSpeed, 540.f, 0.01f));
			TestTrue(TEXT("bCancelWhenNoTarget 应还原"), ImportedFace->bCancelWhenNoTarget);
		}
	}

	return true;
}

/**
 * 新增任务的校验规则：错误必须"无法被配置出来"。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKNewTasksValidateTest, "UAK.Unit.Task.NewTasks.ValidationRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKNewTasksValidateTest::RunTest(const FString& Parameters)
{
	UAKAbilityAsset* Asset = UAKTest::NewAsset(2.f);

	// 1) 没填 Tag 的 ApplyLooseTags 应报错
	{
		FAKValidationResult Result;
		UAKAbilityTask_ApplyLooseTags* Empty = NewObject<UAKAbilityTask_ApplyLooseTags>(Asset);
		Empty->Validate(Asset, Result);
		TestTrue(TEXT("空 Tag 应被判为错误"), Result.NumErrors() >= 1);
	}

	// 2) 填了 Tag 就不该再报错
	{
		FAKValidationResult Result;
		UAKAbilityTask_ApplyLooseTags* Filled = NewObject<UAKAbilityTask_ApplyLooseTags>(Asset);
		Filled->Tags.AddTag(TestLooseTag());
		Filled->Validate(Asset, Result);
		TestEqual(TEXT("填了 Tag 就不该有错误"), Result.NumErrors(), 0);
	}

	// 3) 平滑转向却没勾 NeedsTick → 错误
	{
		FAKValidationResult Result;
		UAKAbilityTask_FaceTarget* SmoothNoTick = NewObject<UAKAbilityTask_FaceTarget>(Asset);
		SmoothNoTick->bInstant = false;
		SmoothNoTick->bNeedsTick = false;
		SmoothNoTick->Validate(Asset, Result);
		TestTrue(TEXT("平滑转向没勾 NeedsTick 应报错"), Result.NumErrors() >= 1);
	}

	// 4) 世界方向为零向量 → 错误
	{
		FAKValidationResult Result;
		UAKAbilityTask_FaceTarget* ZeroDir = NewObject<UAKAbilityTask_FaceTarget>(Asset);
		ZeroDir->Mode = EAKFaceMode::WorldDirection;
		ZeroDir->WorldDirection = FVector::ZeroVector;
		ZeroDir->Validate(Asset, Result);
		TestTrue(TEXT("零向量方向应报错"), Result.NumErrors() >= 1);
	}

	return true;
}

/**
 * ApplyLooseTags 的运行时语义：任务存活期间 Tag 在，任务结束 Tag 消失。
 * 这条用例的价值在于"中断也要摘干净"——残留的霸体 Tag 是很难查的线上 bug。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKLooseTagsRuntimeTest, "UAK.Instance.Task.ApplyLooseTags.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKLooseTagsRuntimeTest::RunTest(const FString& Parameters)
{
	FUAKTestWorldScope World;
	if (!World.Create() || !World.World)
	{
		AddError(TEXT("无法创建测试世界"));
		return false;
	}

	AAKTestGASActor* Actor = UAKTest::SpawnGASActor(World.World, 100.f);
	TestTrue(TEXT("测试载体应存在"), Actor != nullptr);
	if (!Actor)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = Actor->GetAbilitySystemComponent();
	TestTrue(TEXT("ASC 应存在"), ASC != nullptr);
	if (!ASC)
	{
		return false;
	}

	UAKAbilityAsset* Asset = UAKTest::NewAsset(1.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset, EAKTrackType::Effect);
	UAKAbilityTask_ApplyLooseTags* Task = UAKTest::AddTask<UAKAbilityTask_ApplyLooseTags>(Track, 0.f, 0.4f);
	Task->TaskId = FName(TEXT("SuperArmor"));
	Task->Tags.AddTag(TestLooseTag());

	UAKAbilityInstance* Instance = UAKTest::NewInstance(Asset);
	TestTrue(TEXT("实例应创建成功"), Instance != nullptr);
	if (!Instance)
	{
		return false;
	}

	Instance->GetContext().SourceASC = ASC;

	TestFalse(TEXT("开始前不应有 Tag"), ASC->HasMatchingGameplayTag(TestLooseTag()));

	// 推进到任务中段：Tag 应该在
	Instance->Tick(0.1f);
	TestTrue(TEXT("任务存活期间应有 Tag"), ASC->HasMatchingGameplayTag(TestLooseTag()));

	// 推进过任务结束时间：Tag 必须消失
	Instance->Tick(0.5f);
	TestFalse(TEXT("任务结束后 Tag 应被移除"), ASC->HasMatchingGameplayTag(TestLooseTag()));

	return true;
}

/**
 * 中断时也要把 Tag 摘掉（默认行为）。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKLooseTagsInterruptTest, "UAK.Instance.Task.ApplyLooseTags.RemovedOnInterrupt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKLooseTagsInterruptTest::RunTest(const FString& Parameters)
{
	FUAKTestWorldScope World;
	if (!World.Create() || !World.World)
	{
		AddError(TEXT("无法创建测试世界"));
		return false;
	}

	AAKTestGASActor* Actor = UAKTest::SpawnGASActor(World.World, 100.f);
	if (!Actor || !Actor->GetAbilitySystemComponent())
	{
		AddError(TEXT("测试载体创建失败"));
		return false;
	}

	UAbilitySystemComponent* ASC = Actor->GetAbilitySystemComponent();

	UAKAbilityAsset* Asset = UAKTest::NewAsset(1.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset, EAKTrackType::Effect);
	UAKAbilityTask_ApplyLooseTags* Task = UAKTest::AddTask<UAKAbilityTask_ApplyLooseTags>(Track, 0.f, 0.9f);
	Task->Tags.AddTag(TestLooseTag());

	UAKAbilityInstance* Instance = UAKTest::NewInstance(Asset);
	if (!Instance)
	{
		AddError(TEXT("实例创建失败"));
		return false;
	}

	Instance->GetContext().SourceASC = ASC;

	Instance->Tick(0.1f);
	TestTrue(TEXT("任务存活期间应有 Tag"), ASC->HasMatchingGameplayTag(TestLooseTag()));

	Instance->Finish(/*bInterrupted=*/true);
	TestFalse(TEXT("被打断后 Tag 也必须消失"), ASC->HasMatchingGameplayTag(TestLooseTag()));

	return true;
}

#undef LOCTEXT_NAMESPACE
