#include "UAKPreviewRunner.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityInstance.h"
#include "UAKGameplayAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameplayCueNotify_Actor.h"

UAbilitySystemComponent* UUAKPreviewRunner::EnsureAbilitySystem(AActor* Actor)
{
	if (UAbilitySystemComponent* Existing = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor))
	{
		return Existing;
	}

	// 预览世界没有 GameMode，没人替我们挂 ASC —— 这里补上：
	// Cue / GE / Montage 全都经由它落地，缺了它预览就只剩空转。
	UAbilitySystemComponent* NewASC = NewObject<UAbilitySystemComponent>(Actor);
	NewASC->SetIsReplicated(false);
	Actor->AddInstanceComponent(NewASC);
	NewASC->RegisterComponent();

	// 不调这个 AbilityActorInfo 是空的，Cue 会被 UAbilitySystemComponent::InvokeGameplayCueEvent
	// 的 ensure 直接拦掉（这是最容易踩的一步）。
	NewASC->InitAbilityActorInfo(Actor, Actor);

	return NewASC;
}

bool UUAKPreviewRunner::Start(AActor* InPreviewActor, UAKAbilityAsset* InAsset, bool bInLoop)
{
	Reset();

	PreviewActor = InPreviewActor;
	Asset = InAsset;
	bLoop = bInLoop;

	if (!PreviewActor || !Asset)
	{
		return false;
	}

	ASC = EnsureAbilitySystem(PreviewActor);
	if (!ASC)
	{
		return false;
	}

	// 不授予、不激活：只是给 PlayMontage 这类走 ASC 的任务一个非空的 Ability。
	PreviewAbility = NewObject<UAKGameplayAbility>(GetTransientPackage());
	PreviewAbility->AbilityAsset = Asset;

	FAKAbilityContext Context;
	Context.Owner = PreviewActor;
	Context.Instigator = PreviewActor;
	Context.SourceASC = ASC;
	Context.Ability = PreviewAbility;
	Context.PlayRate = Asset->PlayRate;
	// 预览是一次单机施放：两端都置 true，表现类任务（PlayMontage / SendGameplayCue
	// 都是 ClientOnly）才不会被 ShouldRunOnThisEnd() 滤掉。
	Context.bIsAuthority = true;
	Context.bIsLocallyControlled = true;

	Instance = NewObject<UAKAbilityInstance>(this);
	Instance->SetTrace(nullptr);	// 预览暂不记录溯源，血缘视图仍走设计期的 GetGASImpact
	Instance->Initialize(Asset, Context);

	return !Instance->IsFinished();
}

bool UUAKPreviewRunner::Tick(float DeltaTime)
{
	if (!Instance || Instance->IsFinished())
	{
		return false;
	}

	Instance->Tick(DeltaTime);

	if (Instance->IsFinished())
	{
		if (bLoop)
		{
			Restart();
			return !Instance->IsFinished();
		}
		return false;
	}

	return true;
}

void UUAKPreviewRunner::Stop()
{
	if (Instance)
	{
		Instance->Finish(/*bInterrupted=*/true);
		Instance = nullptr;
	}

	PreviewAbility = nullptr;

	if (!PreviewActor)
	{
		return;
	}

	// 收掉表现：Montage 停掉，Add 类 Cue 逐个 Remove（Execute 是一次性的，不用管），
	// Cue 生成的 Actor 直接销毁 —— 预览世界是长期存在的，不收会越积越多。
	if (USkeletalMeshComponent* Mesh = PreviewActor->FindComponentByClass<USkeletalMeshComponent>())
	{
		if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.1f);
		}
	}

	// 清掉"直接添加"的那批 Cue（AddGameplayCue）；GE 携带的 Cue 随角色重建一起消失 ——
	// 每次 Start() 都会重建预览角色，属性 / GE 天然归零，不需要在这里手写回滚。
	if (ASC)
	{
		ASC->RemoveAllGameplayCues();
	}

	if (UWorld* World = PreviewActor->GetWorld())
	{
		TArray<AGameplayCueNotify_Actor*> CueActors;
		for (TActorIterator<AGameplayCueNotify_Actor> It(World); It; ++It)
		{
			CueActors.Add(*It);
		}
		for (AGameplayCueNotify_Actor* CueActor : CueActors)
		{
			CueActor->Destroy();
		}
	}
}

void UUAKPreviewRunner::Reset()
{
	Stop();

	PreviewActor = nullptr;
	Asset = nullptr;
	ASC = nullptr;
}

void UUAKPreviewRunner::Restart()
{
	AActor* Actor = PreviewActor;
	UAKAbilityAsset* AssetToRun = Asset;
	const bool bWasLooping = bLoop;

	// Restart 不重置角色（属性 / GE 由调用方在 Start 前重建时归零），
	// 只重新展开一次执行计划。
	Stop();
	Start(Actor, AssetToRun, bWasLooping);
}

bool UUAKPreviewRunner::IsRunning() const
{
	return Instance != nullptr && !Instance->IsFinished();
}

float UUAKPreviewRunner::GetCurrentTime() const
{
	return Instance ? Instance->GetCurrentTime() : 0.f;
}

float UUAKPreviewRunner::GetLength() const
{
	return Instance ? Instance->GetLength() : 0.f;
}
