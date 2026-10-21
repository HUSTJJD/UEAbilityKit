#include "UAKTestCommon.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityInstance.h"
#include "UAKAbilityTrack.h"
#include "UAKAbilityContext.h"

#include "GameplayEffect.h"
#include "Engine/World.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "UAKTestCommon"

// ────────────────────────────────────────────────────────────────────────
// 测试载体
// ────────────────────────────────────────────────────────────────────────

AAKTestGASActor::AAKTestGASActor()
{
	// 必须有根组件：没有它，SetActorLocation 静默失效、GetActorLocation 永远返回原点，
	// 一切依赖位置的逻辑（距离过滤、范围效果、投射物生成）在测试里都会失真。
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	AbilitySystemComponent->SetIsReplicated(false);

	Attributes = CreateDefaultSubobject<UAKTestAttributeSet>(TEXT("Attributes"));
}

namespace
{
	/** 往 GE 上挂一个对 Health 的 Additive 修正 */
	void AddHealthModifier(UGameplayEffect* Effect, float Delta)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UAKTestAttributeSet::GetHealthAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Delta));
		Effect->Modifiers.Add(Modifier);
	}
}

UAKTestDamageEffect::UAKTestDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddHealthModifier(this, -30.f);
}

UAKTestHealEffect::UAKTestHealEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddHealthModifier(this, 15.f);
}

// ────────────────────────────────────────────────────────────────────────
// 探针任务
// ────────────────────────────────────────────────────────────────────────

TArray<FString>& UAKAbilityTask_TestProbe::GetLog()
{
	static TArray<FString> Log;
	return Log;
}

void UAKAbilityTask_TestProbe::ResetLog()
{
	GetLog().Reset();
}

void UAKAbilityTask_TestProbe::AppendLog(const FString& Line)
{
	GetLog().Add(Line);
}

bool UAKAbilityTask_TestProbe::LogContains(const FString& SubString)
{
	for (const FString& Line : GetLog())
	{
		if (Line.Contains(SubString))
		{
			return true;
		}
	}
	return false;
}

int32 UAKAbilityTask_TestProbe::CountLog(const FString& SubString)
{
	int32 Count = 0;
	for (const FString& Line : GetLog())
	{
		if (Line.Contains(SubString))
		{
			++Count;
		}
	}
	return Count;
}

void UAKAbilityTask_TestProbe::OnTaskStart(FAKAbilityContext& Context)
{
	AppendLog(FString::Printf(TEXT("START %s @%.3f"),
		TaskId != NAME_None ? *TaskId.ToString() : *GetName(), Context.CurrentTime));
}

void UAKAbilityTask_TestProbe::OnTaskTick(FAKAbilityContext& Context, float DeltaTime)
{
	AppendLog(FString::Printf(TEXT("TICK %s @%.3f"),
		TaskId != NAME_None ? *TaskId.ToString() : *GetName(), Context.CurrentTime));
}

void UAKAbilityTask_TestProbe::OnTaskEnd(FAKAbilityContext& Context, bool bInterrupted)
{
	AppendLog(FString::Printf(TEXT("END %s @%.3f interrupted=%d"),
		TaskId != NAME_None ? *TaskId.ToString() : *GetName(), Context.CurrentTime, bInterrupted ? 1 : 0));
}

// ────────────────────────────────────────────────────────────────────────
// 临时世界
// ────────────────────────────────────────────────────────────────────────

/**
 * 所有用例共用同一个 EditorPreview 世界。
 *
 * 为什么不每次新建一个：EditorPreview 世界里的 Actor 要求"全局唯一"名字，
 * 而 InitializeNewWorld 生成 WorldSettings 时用的是 Required_Fatal。
 * 只要进程里已经存在第二个这种世界（哪怕第一个已经 DestroyWorld），
 * 名字就撞了 → 直接 Fatal。
 * 测试要的是"有个世界能 SpawnActor + 跑 GAS"，不是"每个用例一个干净世界"，
 * 所以共享一个是既简单又稳的做法。
 */
namespace
{
	UWorld* GSharedTestWorld = nullptr;
}

bool FUAKTestWorldScope::Create()
{
	if (World)
	{
		return true;
	}

	if (!GSharedTestWorld)
	{
		// EditorPreview 世界不需要注册 WorldContext，也不参与 PIE，最适合跑 GAS 单元测试
		GSharedTestWorld = UWorld::CreateWorld(EWorldType::EditorPreview, /*bInformEngineOfWorld=*/false,
			TEXT("UAKTestWorld"), GetTransientPackage());

		if (GSharedTestWorld)
		{
			// 常驻：进程结束前不销毁，避免后续用例再建世界时名字冲突
			GSharedTestWorld->AddToRoot();
		}
	}

	World = GSharedTestWorld;
	bCreated = false;   // 共享世界不由本作用域销毁
	return World != nullptr;
}

FUAKTestWorldScope::~FUAKTestWorldScope()
{
	if (bCreated && World)
	{
		World->DestroyWorld(/*bBroadcastOnWorldCleanup=*/false);
		World = nullptr;
		bCreated = false;
	}
}

// ────────────────────────────────────────────────────────────────────────
// 辅助函数
// ────────────────────────────────────────────────────────────────────────

namespace UAKTest
{
	UAKAbilityAsset* NewAsset(float Length)
	{
		UAKAbilityAsset* Asset = NewObject<UAKAbilityAsset>(GetTransientPackage(),
			UAKAbilityAsset::StaticClass(), NAME_None, RF_Standalone);
		Asset->Length = Length;
		Asset->PlayRate = 1.f;
		return Asset;
	}

	FString StripJsonAssetName(const FString& Json)
	{
		const FString Key(TEXT("\"asset\": \""));
		const int32 Start = Json.Find(Key);
		if (Start == INDEX_NONE)
		{
			return Json;
		}

		const int32 ValueStart = Start + Key.Len();
		const int32 ValueEnd = Json.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, ValueStart);
		if (ValueEnd == INDEX_NONE)
		{
			return Json;
		}

		return Json.Left(ValueStart) + Json.Mid(ValueEnd);
	}

	UAKAbilityTrack* AddTrack(UAKAbilityAsset* Asset, EAKTrackType TrackType, EAKTrackCompositeMode CompositeMode)
	{
		if (!Asset)
		{
			return nullptr;
		}

		UAKAbilityTrack* Track = NewObject<UAKAbilityTrack>(Asset);
		Track->TrackName = FName(*FString::Printf(TEXT("Track%d"), Asset->Tracks.Num()));
		Track->TrackType = TrackType;
		Track->CompositeMode = CompositeMode;
		Track->bEnabled = true;
		Asset->Tracks.Add(Track);
		return Track;
	}

	UAKAbilityInstance* NewInstance(UAKAbilityAsset* Asset)
	{
		if (!Asset)
		{
			return nullptr;
		}

		FAKAbilityContext Context;
		Context.CurrentTime = 0.f;
		Context.PlayRate = 1.f;
		Context.bIsAuthority = true;
		Context.bIsLocallyControlled = true;

		UAKAbilityInstance* Instance = NewObject<UAKAbilityInstance>(GetTransientPackage(),
			UAKAbilityInstance::StaticClass(), NAME_None, RF_Standalone);
		Instance->Initialize(Asset, Context);
		return Instance;
	}

	AAKTestGASActor* SpawnGASActor(UWorld* World, float Health, float MaxHealth)
	{
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AAKTestGASActor* Actor = World->SpawnActor<AAKTestGASActor>(
			AAKTestGASActor::StaticClass(), FTransform::Identity, Params);
		if (!Actor)
		{
			return nullptr;
		}

		Actor->AbilitySystemComponent->InitAbilityActorInfo(Actor, Actor);
		// 显式注册属性集，避免依赖 Actor 子对象的自动发现顺序
		Actor->AbilitySystemComponent->AddSpawnedAttribute(Actor->Attributes);

		Actor->Attributes->InitMaxHealth(MaxHealth);
		Actor->Attributes->InitHealth(Health);

		return Actor;
	}

	FString GetAutomationOutputDir()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("UAKAutomation"));
	}

	FString GetRunStamp()
	{
		return FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
	}
}

#undef LOCTEXT_NAMESPACE
