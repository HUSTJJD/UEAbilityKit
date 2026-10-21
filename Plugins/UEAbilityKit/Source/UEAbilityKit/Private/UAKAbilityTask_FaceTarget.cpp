#include "UAKAbilityTask_FaceTarget.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityContext.h"

#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "UAKAbilityTask_FaceTarget"

namespace
{
	/** 取第一个还活着的目标 */
	AActor* FirstValidTarget(const FAKAbilityContext& Context)
	{
		for (const TWeakObjectPtr<AActor>& Target : Context.Targets)
		{
			if (Target.IsValid())
			{
				return Target.Get();
			}
		}
		return nullptr;
	}
}

bool UAKAbilityTask_FaceTarget::ComputeTargetRotation(const FAKAbilityContext& Context, FRotator& OutRotation) const
{
	AActor* Owner = Context.Owner.Get();
	if (!Owner)
	{
		return false;
	}

	switch (Mode)
	{
	case EAKFaceMode::FirstTarget:
	{
		if (const AActor* Target = FirstValidTarget(Context))
		{
			OutRotation = (Target->GetActorLocation() - Owner->GetActorLocation()).Rotation();
			return true;
		}
		return false;
	}

	case EAKFaceMode::Velocity:
	{
		const FVector Velocity = Owner->GetVelocity();
		if (Velocity.SizeSquared2D() > KINDA_SMALL_NUMBER)
		{
			OutRotation = Velocity.Rotation();
			return true;
		}
		return false;
	}

	case EAKFaceMode::ControlRotation:
	{
		if (const APawn* Pawn = Cast<APawn>(Owner))
		{
			if (const AController* Controller = Pawn->GetController())
			{
				OutRotation = Controller->GetControlRotation();
				return true;
			}
		}
		return false;
	}

	case EAKFaceMode::WorldDirection:
	default:
	{
		if (WorldDirection.IsNearlyZero())
		{
			return false;
		}
		OutRotation = WorldDirection.Rotation();
		return true;
	}
	}
}

void UAKAbilityTask_FaceTarget::ApplyRotation(AActor* Owner, const FRotator& TargetRotation) const
{
	if (!Owner)
	{
		return;
	}

	FRotator NewRotation = Owner->GetActorRotation();
	if (bOnlyYaw)
	{
		NewRotation.Yaw = TargetRotation.Yaw;
	}
	else
	{
		NewRotation = TargetRotation;
	}

	Owner->SetActorRotation(NewRotation);
}

bool UAKAbilityTask_FaceTarget::ShouldEndEarly(const FAKAbilityContext& Context) const
{
	if (!bCancelWhenNoTarget || bInstant)
	{
		return false;
	}

	const UAKFaceTargetScratchPad* Pad = Cast<UAKFaceTargetScratchPad>(Context.ScratchPads.FindRef(this));
	return Pad && !Pad->bHasTarget;
}

void UAKAbilityTask_FaceTarget::OnTaskStart(FAKAbilityContext& Context)
{
	AActor* Owner = Context.Owner.Get();
	if (!Owner)
	{
		return;
	}

	// 朝向是"表现 + 判定"双重属性：权威端改了会复制，本地端改了能立刻响应，
	// 因此两端都执行；纯模拟端（既非权威也非本地）不参与。
	if (!Context.bIsAuthority && !Context.bIsLocallyControlled)
	{
		return;
	}

	FRotator TargetRotation = FRotator::ZeroRotator;
	const bool bHasTarget = ComputeTargetRotation(Context, TargetRotation);

	if (!bHasTarget)
	{
		return;
	}

	if (bInstant)
	{
		ApplyRotation(Owner, TargetRotation);
		return;
	}

	if (UAKFaceTargetScratchPad* Pad = Cast<UAKFaceTargetScratchPad>(Context.ScratchPads.FindRef(this)))
	{
		Pad->TargetRotation = TargetRotation;
		Pad->bHasTarget = true;
	}
}

void UAKAbilityTask_FaceTarget::OnTaskTick(FAKAbilityContext& Context, float DeltaTime)
{
	if (bInstant)
	{
		return;
	}

	AActor* Owner = Context.Owner.Get();
	if (!Owner)
	{
		return;
	}

	if (!Context.bIsAuthority && !Context.bIsLocallyControlled)
	{
		return;
	}

	UAKFaceTargetScratchPad* Pad = Cast<UAKFaceTargetScratchPad>(Context.ScratchPads.FindRef(this));
	if (!Pad)
	{
		return;
	}

	// 目标会动：每帧重算朝向，再按角速度逼近
	FRotator Desired = Pad->TargetRotation;
	if (ComputeTargetRotation(Context, Desired))
	{
		Pad->TargetRotation = Desired;
		Pad->bHasTarget = true;
	}

	if (!Pad->bHasTarget)
	{
		return;
	}

	const FRotator Current = Owner->GetActorRotation();
	const float MaxStep = TurnSpeed * DeltaTime;

	FRotator NewRotation = Current;
	if (bOnlyYaw)
	{
		// FindDeltaAngleDegrees 处理 ±180 跨越，避免绕远路
		const float Delta = FMath::FindDeltaAngleDegrees(Current.Yaw, Pad->TargetRotation.Yaw);
		NewRotation.Yaw = Current.Yaw + FMath::Clamp(Delta, -MaxStep, MaxStep);
	}
	else
	{
		NewRotation = FMath::RInterpConstantTo(Current, Pad->TargetRotation, DeltaTime, TurnSpeed);
	}

	Owner->SetActorRotation(NewRotation);
}

UObject* UAKAbilityTask_FaceTarget::CreateScratchPad(const FAKAbilityContext& Context)
{
	// 瞬时转向不需要状态；平滑转向要记住目标朝向
	return bInstant ? nullptr : NewObject<UAKFaceTargetScratchPad>();
}

void UAKAbilityTask_FaceTarget::Validate(const UAKAbilityAsset* Asset, FAKValidationResult& OutResult) const
{
	Super::Validate(Asset, OutResult);

	if (!bInstant && !bNeedsTick)
	{
		OutResult.Add(EAKValidationSeverity::Error,
			LOCTEXT("SmoothNeedsTick", "平滑转向需要每帧驱动：请勾选 NeedsTick，或改用瞬时转向。"),
			const_cast<UAKAbilityTask_FaceTarget*>(this));
	}

	if (Mode == EAKFaceMode::WorldDirection && WorldDirection.IsNearlyZero())
	{
		OutResult.Add(EAKValidationSeverity::Error,
			LOCTEXT("ZeroDirection", "世界方向模式下的方向向量不能为零。"),
			const_cast<UAKAbilityTask_FaceTarget*>(this));
	}

	if (Mode == EAKFaceMode::FirstTarget && !bCancelWhenNoTarget)
	{
		OutResult.Add(EAKValidationSeverity::Info,
			LOCTEXT("NoTargetBehaviour", "没有目标时保持当前朝向（需要「没目标就放弃」请勾选 CancelWhenNoTarget）。"),
			const_cast<UAKAbilityTask_FaceTarget*>(this));
	}
}

void UAKAbilityTask_FaceTarget::ExportJson(TSharedRef<FJsonObject> OutJson) const
{
	Super::ExportJson(OutJson);

	OutJson->SetStringField(TEXT("mode"),
		StaticEnum<EAKFaceMode>()->GetNameStringByValue(static_cast<int64>(Mode)));
	OutJson->SetBoolField(TEXT("instant"), bInstant);
	OutJson->SetNumberField(TEXT("turnSpeed"), TurnSpeed);
	OutJson->SetBoolField(TEXT("onlyYaw"), bOnlyYaw);
	OutJson->SetBoolField(TEXT("cancelWhenNoTarget"), bCancelWhenNoTarget);

	if (Mode == EAKFaceMode::WorldDirection)
	{
		TArray<TSharedPtr<FJsonValue>> Components;
		Components.Add(MakeShared<FJsonValueNumber>(WorldDirection.X));
		Components.Add(MakeShared<FJsonValueNumber>(WorldDirection.Y));
		Components.Add(MakeShared<FJsonValueNumber>(WorldDirection.Z));
		OutJson->SetArrayField(TEXT("worldDirection"), Components);
	}
}

void UAKAbilityTask_FaceTarget::ImportJson(const FJsonObject& InJson)
{
	Super::ImportJson(InJson);

	FString ModeName;
	if (InJson.TryGetStringField(TEXT("mode"), ModeName))
	{
		if (const UEnum* Enum = StaticEnum<EAKFaceMode>())
		{
			Mode = static_cast<EAKFaceMode>(Enum->GetValueByNameString(ModeName));
		}
	}

	bool bOutInstant = bInstant;
	if (InJson.TryGetBoolField(TEXT("instant"), bOutInstant))
	{
		bInstant = bOutInstant;
	}

	double OutSpeed = TurnSpeed;
	if (InJson.TryGetNumberField(TEXT("turnSpeed"), OutSpeed))
	{
		TurnSpeed = static_cast<float>(OutSpeed);
	}

	bool bOutOnlyYaw = bOnlyYaw;
	if (InJson.TryGetBoolField(TEXT("onlyYaw"), bOutOnlyYaw))
	{
		bOnlyYaw = bOutOnlyYaw;
	}

	bool bOutCancel = bCancelWhenNoTarget;
	if (InJson.TryGetBoolField(TEXT("cancelWhenNoTarget"), bOutCancel))
	{
		bCancelWhenNoTarget = bOutCancel;
	}

	if (const TArray<TSharedPtr<FJsonValue>>* Components = nullptr;
		InJson.TryGetArrayField(TEXT("worldDirection"), Components) && Components && Components->Num() >= 3)
	{
		WorldDirection = FVector(
			(*Components)[0]->AsNumber(),
			(*Components)[1]->AsNumber(),
			(*Components)[2]->AsNumber());
	}
}

#undef LOCTEXT_NAMESPACE
