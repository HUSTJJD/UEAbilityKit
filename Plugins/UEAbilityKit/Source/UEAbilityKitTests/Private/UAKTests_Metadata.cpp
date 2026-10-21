#include "Misc/AutomationTest.h"

#include "UAKTestCommon.h"

#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTask_ApplyGameplayEffect.h"
#include "UAKAbilityTask_ApplyLooseTags.h"
#include "UAKAbilityTask_AreaEffect.h"
#include "UAKAbilityTask_Branch.h"
#include "UAKAbilityTask_CollisionQuery.h"
#include "UAKAbilityTask_Dash.h"
#include "UAKAbilityTask_FaceTarget.h"
#include "UAKAbilityTask_PlayMontage.h"
#include "UAKAbilityTask_SendGameplayCue.h"
#include "UAKAbilityTask_SpawnProjectile.h"
#include "UAKAbilityTask_WaitUntil.h"

#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "UAKTests.Metadata"

/**
 * 所有内置任务都必须有可用的编辑器元数据。
 *
 * 元数据（分类 / 显示名 / 说明 / 配色）集中在一张表里，
 * 一旦新增任务忘了登记，任务选择器里就会出现空白条目——这类回归只能靠测试兜住。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKMetadataBuiltinTasksTest, "UAK.Unit.Metadata.BuiltinTasksAreAnnotated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKMetadataBuiltinTasksTest::RunTest(const FString& Parameters)
{
	const UClass* Probes[] =
	{
		UAKAbilityTask_ApplyGameplayEffect::StaticClass(),
		UAKAbilityTask_ApplyLooseTags::StaticClass(),
		UAKAbilityTask_AreaEffect::StaticClass(),
		UAKAbilityTask_Branch::StaticClass(),
		UAKAbilityTask_CollisionQuery::StaticClass(),
		UAKAbilityTask_Dash::StaticClass(),
		UAKAbilityTask_FaceTarget::StaticClass(),
		UAKAbilityTask_PlayMontage::StaticClass(),
		UAKAbilityTask_SendGameplayCue::StaticClass(),
		UAKAbilityTask_SpawnProjectile::StaticClass(),
		UAKAbilityTask_WaitUntil::StaticClass(),
	};

	int32 Checked = 0;

	for (const UClass* Class : Probes)
	{
		if (!Class)
		{
			continue;
		}

		const UAKAbilityTaskBase* DefaultTask = Class->GetDefaultObject<UAKAbilityTaskBase>();
		if (!DefaultTask)
		{
			AddError(FString::Printf(TEXT("任务类 %s 取不到 CDO"), *Class->GetName()));
			continue;
		}

		const FString ClassName = Class->GetName();

		// 把解析结果写进报告：元数据表没命中时，显示名会回落到"去前缀类名"，
		// 一眼就能看出是"表没找到"还是"文案没填"。
		AddInfo(FString::Printf(TEXT("%s → 分类=%s | 显示名=%s | 说明=%s"),
			*ClassName, *DefaultTask->GetTaskCategory().ToString(),
			*DefaultTask->GetTaskDisplayName().ToString(), *DefaultTask->GetTaskDescription().ToString()));

		TestFalse(FString::Printf(TEXT("%s 的显示名不应为空"), *ClassName),
			DefaultTask->GetTaskDisplayName().IsEmpty());
		TestFalse(FString::Printf(TEXT("%s 的分类不应为空"), *ClassName),
			DefaultTask->GetTaskCategory().IsEmpty());
		TestFalse(FString::Printf(TEXT("%s 的说明不应为空"), *ClassName),
			DefaultTask->GetTaskDescription().IsEmpty());
		TestFalse(FString::Printf(TEXT("%s 不应被标记为废弃"), *ClassName),
			DefaultTask->IsDeprecated());

		++Checked;
	}

	TestTrue(TEXT("应至少检查 11 个内置任务"), Checked >= 11);
	return true;
}

/**
 * 未登记的任务（例如测试用的探针）应回落到"类名去前缀"，
 * 而不是显示空白——这是元数据表查找失败时的兜底行为。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKMetadataFallbackTest, "UAK.Unit.Metadata.FallbackToClassName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKMetadataFallbackTest::RunTest(const FString& Parameters)
{
	const UAKAbilityTask_TestProbe* Probe = UAKAbilityTask_TestProbe::StaticClass()
		->GetDefaultObject<UAKAbilityTask_TestProbe>();

	TestTrue(TEXT("探针任务应存在"), Probe != nullptr);
	if (!Probe)
	{
		return false;
	}

	const FString DisplayName = Probe->GetTaskDisplayName().ToString();

	TestFalse(TEXT("未登记的任务也应有显示名（回落到类名）"), DisplayName.IsEmpty());
	TestTrue(TEXT("回落名应去掉 UAKAbilityTask_ 前缀"), !DisplayName.Contains(TEXT("UAKAbilityTask_")));

	return true;
}

/**
 * 血缘视图与 GAS 影响条依赖 GetGASImpact()——它必须"不运行游戏也能说出来"。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKMetadataGasImpactTest, "UAK.Unit.Metadata.GasImpactIsStatic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKMetadataGasImpactTest::RunTest(const FString& Parameters)
{
	UAKAbilityTask_ApplyGameplayEffect* Task = NewObject<UAKAbilityTask_ApplyGameplayEffect>(GetTransientPackage());
	Task->EffectClass = UAKTestDamageEffect::StaticClass();
	Task->SetByCaller.Add(FName(TEXT("Damage")), 30.f);

	FAKGASImpact Impact;
	Task->GetGASImpact(Impact);

	TestEqual(TEXT("应报告 1 个 GE"), Impact.Effects.Num(), 1);
	TestFalse(TEXT("摘要不应为空"), Impact.Summary.IsEmpty());
	TestTrue(TEXT("摘要里应带上 SetByCaller 数值"), Impact.Summary.ToString().Contains(TEXT("Damage")));

	return true;
}

#undef LOCTEXT_NAMESPACE
