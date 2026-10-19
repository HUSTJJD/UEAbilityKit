#include "UAKSampleCharacter.h"

#include "UAKSampleAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

AAKSampleCharacter::AAKSampleCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// 玩家用 Mixed（属性复制到拥有者、GE 复制给所有人）；AI 用 Minimal
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UAKSampleAttributeSet>(TEXT("AttributeSet"));
}

void AAKSampleCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority() && IsLocallyControlled())
	{
		// 客户端在 PlayerState 到达后再初始化一次
	}
	InitAbilityActorInfo();
}

void AAKSampleCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 服务端：这里初始化并授予技能
	InitAbilityActorInfo();
	GrantDefaultAbilities();
}

void AAKSampleCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 客户端：PlayerState 到达后初始化
	InitAbilityActorInfo();
}

void AAKSampleCharacter::InitAbilityActorInfo()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	bAbilityActorInfoInitialized = true;

	if (DefaultAttributesEffect)
	{
		const FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(DefaultAttributesEffect, 1.f, Context);
		if (Spec.IsValid())
		{
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
}

void AAKSampleCharacter::GrantDefaultAbilities()
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (!AbilityClass)
		{
			continue;
		}

		AbilitySystemComponent->GiveAbility(
			FGameplayAbilitySpec(AbilityClass, /*Level=*/1, /*InputID=*/INDEX_NONE, this));
	}
}
