#include "UAKDemoCharacter.h"

#include "UAKDemoAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "Components/InputComponent.h"
#include "GameplayEffect.h"

AAKDemoCharacter::AAKDemoCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// 玩家用 Mixed（属性复制到拥有者、GE 复制给所有人）；AI 用 Minimal
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UAKDemoAttributeSet>(TEXT("AttributeSet"));
}

FString AAKDemoCharacter::GetModuleName_Implementation() const
{
	// 对应 Content/Script/UAKDemoCharacter.lua
	return TEXT("UAKDemoCharacter");
}

void AAKDemoCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority() && IsLocallyControlled())
	{
		// 客户端在 PlayerState 到达后再初始化一次
	}
	InitAbilityActorInfo();
}

void AAKDemoCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 服务端：这里初始化并授予技能
	InitAbilityActorInfo();
	GrantDefaultAbilities();
}

void AAKDemoCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 客户端：PlayerState 到达后初始化
	InitAbilityActorInfo();
}

void AAKDemoCharacter::InitAbilityActorInfo()
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

void AAKDemoCharacter::GrantDefaultAbilities()
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

// ── 输入 ────────────────────────────────────────────────────────────
void AAKDemoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!PlayerInputComponent)
	{
		return;
	}

	// 旧式动作映射：UAK_Ability1 / UAK_Ability2（键位见 Config/DefaultInput.ini）。
	// 只负责"转发按下/松开"，按下之后干什么由 Lua 的 OnAbilityInput 决定。
	PlayerInputComponent->BindAction(TEXT("UAK_Ability1"), IE_Pressed, this, &AAKDemoCharacter::HandleAbility1Pressed);
	PlayerInputComponent->BindAction(TEXT("UAK_Ability1"), IE_Released, this, &AAKDemoCharacter::HandleAbility1Released);
	PlayerInputComponent->BindAction(TEXT("UAK_Ability2"), IE_Pressed, this, &AAKDemoCharacter::HandleAbility2Pressed);
	PlayerInputComponent->BindAction(TEXT("UAK_Ability2"), IE_Released, this, &AAKDemoCharacter::HandleAbility2Released);
}

void AAKDemoCharacter::HandleAbility1Pressed() { NotifyAbilityInput(0, true); }
void AAKDemoCharacter::HandleAbility1Released() { NotifyAbilityInput(0, false); }
void AAKDemoCharacter::HandleAbility2Pressed() { NotifyAbilityInput(1, true); }
void AAKDemoCharacter::HandleAbility2Released() { NotifyAbilityInput(1, false); }

void AAKDemoCharacter::NotifyAbilityInput(int32 SlotIndex, bool bPressed)
{
	// 具体行为（激活哪个技能、UI 怎么反馈）交给脚本
	K2_OnAbilityInput(SlotIndex, bPressed);
}

bool AAKDemoCharacter::TryActivateAbilityByIndex(int32 Index)
{
	if (!AbilitySystemComponent)
	{
		return false;
	}

	if (!DefaultAbilities.IsValidIndex(Index) || !DefaultAbilities[Index])
	{
		return false;
	}

	return AbilitySystemComponent->TryActivateAbilityByClass(DefaultAbilities[Index]);
}

void AAKDemoCharacter::SetChannelingInputHeld(int32 SlotIndex, bool bHeld)
{
	if (bHeld)
	{
		ReleasedInputSlots.Remove(SlotIndex);
	}
	else
	{
		ReleasedInputSlots.Add(SlotIndex);
	}
}

bool AAKDemoCharacter::IsChannelingInputHeld(int32 SlotIndex) const
{
	return !ReleasedInputSlots.Contains(SlotIndex);
}
