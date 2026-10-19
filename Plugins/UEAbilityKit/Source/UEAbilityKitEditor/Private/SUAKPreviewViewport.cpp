#include "SUAKPreviewViewport.h"

#include "UAKAbilityAsset.h"

#include "Animation/AnimMontage.h"
#include "Animation/SkeletalMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "SUAKPreviewViewport"

FAKPreviewViewportClient::FAKPreviewViewportClient(FPreviewScene& InPreviewScene,
	const TSharedRef<SEditorViewport>& InViewportWidget)
	: FEditorViewportClient(&InPreviewScene.GetEditorModeTools(), &InPreviewScene, InViewportWidget)
	, PreviewScene(&InPreviewScene)
{
	SetViewMode(VMI_Lit);
	SetViewportType(LVT_Perspective);
	SetViewLocation(FVector(300.f, 0.f, 120.f));
	SetViewRotation(FRotator(-10.f, 180.f, 0.f));
}

void FAKPreviewViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	if (!PreviewScene)
	{
		return;
	}

	PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);

	if (!bPlaying)
	{
		return;
	}

	PreviewTime += DeltaSeconds;

	AActor* Actor = PreviewActor.Get();
	if (!Actor)
	{
		return;
	}

	if (USkeletalMeshComponent* Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>())
	{
		if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			if (!AnimInstance->IsAnyMontagePlaying())
			{
				if (bLooping)
				{
					AnimInstance->Montage_Resume(nullptr);
				}
				else
				{
					bPlaying = false;
				}
			}
		}
	}
}

FLinearColor FAKPreviewViewportClient::GetBackgroundColor() const
{
	return FLinearColor(0.06f, 0.06f, 0.07f);
}

void FAKPreviewViewportClient::RefreshPreviewActor(UAKAbilityAsset* InAsset)
{
	if (!PreviewScene || !InAsset)
	{
		return;
	}

	// 清理旧预览角色
	if (AActor* OldActor = PreviewActor.Get())
	{
		PreviewScene->RemoveActor(OldActor);
		PreviewActor = nullptr;
	}

	TSubclassOf<AActor> PreviewClass = InAsset->PreviewCharacterClass.LoadSynchronous();
	if (!PreviewClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags = RF_Transient;

	AActor* NewActor = PreviewScene->GetWorld()->SpawnActor<AActor>(PreviewClass, FTransform::Identity, SpawnParams);
	if (!NewActor)
	{
		return;
	}

	PreviewScene->AddActor(NewActor, /*bIsPreview=*/false);
	PreviewActor = NewActor;
	PreviewTime = 0.f;
}

void FAKPreviewViewportClient::PlayPreviewMontage(UAnimMontage* Montage)
{
	AActor* Actor = PreviewActor.Get();
	if (!Actor || !Montage)
	{
		return;
	}

	if (USkeletalMeshComponent* Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>())
	{
		if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			AnimInstance->Montage_Play(Montage);
			bPlaying = true;
			PreviewTime = 0.f;
		}
	}
}

void FAKPreviewViewportClient::StopPreview()
{
	bPlaying = false;

	AActor* Actor = PreviewActor.Get();
	if (!Actor)
	{
		return;
	}

	if (USkeletalMeshComponent* Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>())
	{
		if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.1f);
		}
	}
}

SUAKEditorViewport::SUAKEditorViewport()
	: PreviewScene(MakeShared<FPreviewScene>(FPreviewScene::ConstructionValues()))
{
}

SUAKEditorViewport::~SUAKEditorViewport()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport = nullptr;
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SUAKEditorViewport::Construct(const FArguments& InArgs)
{
	Asset = InArgs._Asset;

	SEditorViewport::Construct(SEditorViewport::FArguments());

	if (UAKAbilityAsset* AssetPtr = Asset.Get())
	{
		ViewportClient->RefreshPreviewActor(AssetPtr);
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<FEditorViewportClient> SUAKEditorViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShared<FAKPreviewViewportClient>(*PreviewScene, SharedThis(this));
	ViewportClient->SetLooping(bLooping);
	return ViewportClient.ToSharedRef();
}

void SUAKEditorViewport::OnAssetChanged(UAKAbilityAsset* InAsset)
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->RefreshPreviewActor(InAsset);
	}
}

FReply SUAKEditorViewport::OnPlayClicked()
{
	if (UAKAbilityAsset* AssetPtr = Asset.Get())
	{
		if (!ViewportClient.IsValid())
		{
			return FReply::Handled();
		}

		// 优先播放任务里的 Montage，其次用资产的预览 Montage
		UAnimMontage* Montage = AssetPtr->PreviewMontage.LoadSynchronous();
		if (Montage)
		{
			ViewportClient->PlayPreviewMontage(Montage);
		}
		else
		{
			ViewportClient->SetPlaying(true);
		}
	}
	return FReply::Handled();
}

FReply SUAKEditorViewport::OnStopClicked()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->StopPreview();
	}
	return FReply::Handled();
}

void SUAKEditorViewport::ToggleLooping()
{
	bLooping = !bLooping;
	if (ViewportClient.IsValid())
	{
		ViewportClient->SetLooping(bLooping);
	}
}

const FSlateBrush* SUAKEditorViewport::GetLoopBrush() const
{
	return FAppStyle::GetBrush(bLooping ? TEXT("Icons.Loop") : TEXT("Icons.Redo"));
}

#undef LOCTEXT_NAMESPACE
