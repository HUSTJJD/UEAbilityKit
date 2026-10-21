#include "Misc/AutomationTest.h"

#include "UAKTestCommon.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityInstance.h"
#include "UAKAbilityTrack.h"
#include "UAKAbilityTask_ApplyGameplayEffect.h"
#include "UAKCondition.h"
#include "UAKQueryFilter.h"

#include "AbilitySystemBlueprintLibrary.h"

#define LOCTEXT_NAMESPACE "UAKTests.GAS"

/**
 * 端到端：UAK 任务 → GE → 目标属性。
 *
 * 这条链是 UAK 的核心承诺——"技能时间轴上一个 ApplyGE 任务，
 * 最终就是 GAS 里一次带预测、带复制、带堆叠的 GE 施加"。
 * 因此这里不 mock 任何东西，真的建 ASC、真的施加、真的读属性。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKGASApplyEffectToTargetTest, "UAK.GAS.ApplyEffect.ToTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKGASApplyEffectToTargetTest::RunTest(const FString& Parameters)
{
	FUAKTestWorldScope World;
	TestTrue(TEXT("测试世界应创建成功"), World.Create());
	if (!World.World)
	{
		return false;
	}

	AAKTestGASActor* Source = UAKTest::SpawnGASActor(World.World, /*Health=*/100.f);
	AAKTestGASActor* Target = UAKTest::SpawnGASActor(World.World, /*Health=*/100.f);

	TestTrue(TEXT("施法者应创建成功"), Source != nullptr);
	TestTrue(TEXT("目标应创建成功"), Target != nullptr);
	if (!Source || !Target)
	{
		return false;
	}

	UAKAbilityAsset* Asset = UAKTest::NewAsset(1.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);

	UAKAbilityTask_ApplyGameplayEffect* ApplyEffect =
		UAKTest::AddTask<UAKAbilityTask_ApplyGameplayEffect>(Track, 0.f, 0.f);
	ApplyEffect->EffectClass = UAKTestDamageEffect::StaticClass();
	ApplyEffect->TaskId = FName(TEXT("Damage"));
	ApplyEffect->SetByCaller.Add(FName(TEXT("Damage")), 30.f);

	UAKAbilityInstance* Instance = UAKTest::NewInstance(Asset);
	if (!Instance)
	{
		AddError(TEXT("实例创建失败"));
		return false;
	}

	FAKAbilityContext& Context = Instance->GetContext();
	Context.Owner = Source;
	Context.SourceASC = Source->GetAbilitySystemComponent();
	Context.Targets.Add(Target);

	Instance->Tick(0.1f);

	TestTrue(TEXT("目标应受伤 30（100 → 70）"),
		FMath::IsNearlyEqual(Target->Attributes->GetHealth(), 70.f, 0.001f));
	TestTrue(TEXT("施法者自身不应受伤"),
		FMath::IsNearlyEqual(Source->Attributes->GetHealth(), 100.f, 0.001f));

	return true;
}

/** 没有目标时按 bApplyToSelfIfNoTarget 施加到施法者自身 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKGASApplyEffectToSelfTest, "UAK.GAS.ApplyEffect.ToSelfWhenNoTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKGASApplyEffectToSelfTest::RunTest(const FString& Parameters)
{
	FUAKTestWorldScope World;
	TestTrue(TEXT("测试世界应创建成功"), World.Create());
	if (!World.World)
	{
		return false;
	}

	AAKTestGASActor* Source = UAKTest::SpawnGASActor(World.World, /*Health=*/50.f);
	if (!Source)
	{
		AddError(TEXT("施法者创建失败"));
		return false;
	}

	UAKAbilityAsset* Asset = UAKTest::NewAsset(1.f);
	UAKAbilityTrack* Track = UAKTest::AddTrack(Asset);

	UAKAbilityTask_ApplyGameplayEffect* ApplyEffect =
		UAKTest::AddTask<UAKAbilityTask_ApplyGameplayEffect>(Track, 0.f, 0.f);
	ApplyEffect->EffectClass = UAKTestHealEffect::StaticClass();
	ApplyEffect->bApplyToSelfIfNoTarget = true;

	UAKAbilityInstance* Instance = UAKTest::NewInstance(Asset);
	if (!Instance)
	{
		AddError(TEXT("实例创建失败"));
		return false;
	}

	FAKAbilityContext& Context = Instance->GetContext();
	Context.Owner = Source;
	Context.SourceASC = Source->GetAbilitySystemComponent();
	// 故意不填 Targets

	Instance->Tick(0.1f);

	TestTrue(TEXT("无目标时应治疗自身 15（50 → 65）"),
		FMath::IsNearlyEqual(Source->Attributes->GetHealth(), 65.f, 0.001f));

	return true;
}

/** 条件判定：基于 GAS 属性（Health ≥ 50） */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKGASConditionTest, "UAK.GAS.Condition.EvaluateOnAttribute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKGASConditionTest::RunTest(const FString& Parameters)
{
	FUAKTestWorldScope World;
	TestTrue(TEXT("测试世界应创建成功"), World.Create());
	if (!World.World)
	{
		return false;
	}

	AAKTestGASActor* Source = UAKTest::SpawnGASActor(World.World, /*Health=*/100.f);
	AAKTestGASActor* LowHealthTarget = UAKTest::SpawnGASActor(World.World, /*Health=*/20.f);
	if (!Source || !LowHealthTarget)
	{
		AddError(TEXT("测试载体创建失败"));
		return false;
	}

	FAKAbilityContext Context;
	Context.Owner = Source;
	Context.SourceASC = Source->GetAbilitySystemComponent();
	Context.bIsAuthority = true;

	FAKCondition Condition;
	Condition.Attribute = UAKTestAttributeSet::GetHealthAttribute();
	Condition.Comparison = EAKComparison::GreaterOrEqual;
	Condition.Threshold = 50.f;

	TestFalse(TEXT("空条件不应判定为成立"), Condition.IsEmpty());
	TestTrue(TEXT("施法者 100 ≥ 50 应成立"), Condition.Evaluate(Context));

	// 同一个条件，改成看第一个目标（20 血）→ 不成立
	FAKCondition TargetCondition = Condition;
	TargetCondition.bEvaluateOnFirstTarget = true;
	Context.Targets.Add(LowHealthTarget);

	TestTrue(TEXT("目标 20 < 50 应不成立（返回 false）"), !TargetCondition.Evaluate(Context));

	return true;
}

/** 过滤器：与形状解耦，"哪些算数"可以单独测 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKGASDistanceFilterTest, "UAK.Unit.Filter.Distance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKGASDistanceFilterTest::RunTest(const FString& Parameters)
{
	FUAKTestWorldScope World;
	TestTrue(TEXT("测试世界应创建成功"), World.Create());
	if (!World.World)
	{
		return false;
	}

	AAKTestGASActor* Source = UAKTest::SpawnGASActor(World.World, 100.f);
	AAKTestGASActor* Near = UAKTest::SpawnGASActor(World.World, 100.f);
	AAKTestGASActor* Far = UAKTest::SpawnGASActor(World.World, 100.f);

	if (!Source || !Near || !Far)
	{
		AddError(TEXT("测试载体创建失败"));
		return false;
	}

	Source->SetActorLocation(FVector::ZeroVector);
	Near->SetActorLocation(FVector(200.f, 0.f, 0.f));
	Far->SetActorLocation(FVector(900.f, 0.f, 0.f));

	UAKQueryFilter_Distance* Filter = NewObject<UAKQueryFilter_Distance>(GetTransientPackage());
	Filter->MaxDistance = 300.f;

	// 位置没生效是这类用例最常见的坑（世界类型 / 组件未注册），先把实际坐标写进报告
	AddInfo(FString::Printf(TEXT("Source=%s Near=%s Far=%s"),
		*Source->GetActorLocation().ToString(), *Near->GetActorLocation().ToString(),
		*Far->GetActorLocation().ToString()));

	TestTrue(TEXT("200cm 应通过"), Filter->Passes(Near, Source));
	TestFalse(TEXT("900cm 应被过滤"), Filter->Passes(Far, Source));
	TestFalse(TEXT("nullptr 候选应被过滤"), Filter->Passes(nullptr, Source));

	// 水平距离模式：高度差不计入
	AAKTestGASActor* Above = UAKTest::SpawnGASActor(World.World, 100.f);
	if (Above)
	{
		Above->SetActorLocation(FVector(0.f, 0.f, 800.f));

		UAKQueryFilter_Distance* FlatFilter = NewObject<UAKQueryFilter_Distance>(GetTransientPackage());
		FlatFilter->MaxDistance = 300.f;
		FlatFilter->bUse2DDistance = true;

		TestTrue(TEXT("只算水平距离时，正上方 800cm 也算近距离"), FlatFilter->Passes(Above, Source));
		TestFalse(TEXT("算 3D 距离时，正上方 800cm 应被过滤"), Filter->Passes(Above, Source));
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
