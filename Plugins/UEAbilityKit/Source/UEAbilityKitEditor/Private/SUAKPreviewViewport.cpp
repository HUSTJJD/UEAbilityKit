#include "SUAKPreviewViewport.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTask_PlayMontage.h"

#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameMapsSettings.h"
#include "Editor.h"           // GLevelEditorModeTools
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "SUAKPreviewViewport"

namespace
{
	/**
	 * 预览角色的默认值链：资产手填 > 项目默认 GameMode 的 DefaultPawnClass > 插件自带示例角色。
	 *
	 * 手填是"我要覆盖"，不是"必须填"：没有理由让每个技能资产再选一次角色，
	 * 留空时编辑器必须自己找到能跑的东西，否则预览 Tab 永远是一片空白。
	 *
	 * 项目那一档读的是 /Script/EngineSettings.GameMapsSettings 的 GlobalDefaultGameMode
	 * —— 任何工程都配过它，且这是纯引擎概念，插件不需要认识具体游戏。
	 */
	TSubclassOf<AActor> ResolvePreviewCharacterClass(const UAKAbilityAsset* Asset)
	{
		if (Asset)
		{
			if (UClass* Explicit = Asset->PreviewCharacterClass.LoadSynchronous())
			{
				return Explicit;
			}
		}

		if (const UGameMapsSettings* MapsSettings = GetDefault<UGameMapsSettings>())
		{
			if (UClass* GameModeClass = MapsSettings->GlobalDefaultGameMode.TryLoadClass<AGameModeBase>())
			{
				if (const AGameModeBase* GameModeCDO = Cast<AGameModeBase>(GameModeClass->GetDefaultObject()))
				{
					if (GameModeCDO->DefaultPawnClass)
					{
						return GameModeCDO->DefaultPawnClass;
					}
				}
			}
		}

		// 插件自带的示例，全部软加载 —— 不为了一个默认值硬依赖示例模块。
		// 先是 commandlet 生成的蓝图角色（网格 + 默认技能都在里面），再退到 C++ 基类。
		static const TCHAR* FallbackPaths[] =
		{
			TEXT("/UEAbilityKit/Demo/BP_DemoCharacter.BP_DemoCharacter_C"),
			TEXT("/Script/UAKDemo.AKDemoCharacter"),
		};
		for (const TCHAR* Path : FallbackPaths)
		{
			if (UClass* Loaded = LoadClass<AActor>(nullptr, Path))
			{
				return Loaded;
			}
		}

		return nullptr;
	}

	/**
	 * 预览 Montage 的默认值链：资产手填 > 资产里第一个 PlayMontage 任务。
	 *
	 * 技能资产里九成带 PlayMontage 任务，让人再填一遍同一个 Montage 没有意义。
	 */
	UAnimMontage* ResolvePreviewMontage(const UAKAbilityAsset* Asset)
	{
		if (!Asset)
		{
			return nullptr;
		}

		if (UAnimMontage* Explicit = Asset->PreviewMontage.LoadSynchronous())
		{
			return Explicit;
		}

		TArray<UAKAbilityTaskBase*> Tasks;
		Asset->CollectTasks(Tasks, /*bOnlyEnabledTracks=*/true);
		for (const UAKAbilityTaskBase* Task : Tasks)
		{
			if (const UAKAbilityTask_PlayMontage* MontageTask = Cast<UAKAbilityTask_PlayMontage>(Task))
			{
				if (MontageTask->Montage)
				{
					return MontageTask->Montage;
				}
			}
		}

		return nullptr;
	}
}

FAKPreviewViewportClient::FAKPreviewViewportClient(FPreviewScene& InPreviewScene,
	const TSharedRef<SEditorViewport>& InViewportWidget)
	// UE 5.8 的 FPreviewScene 不再自带 FEditorModeTools，改用关卡编辑器的全局实例；
	// 第三个参数也从 TSharedRef 变成了 TWeakPtr。
	: FEditorViewportClient(&GLevelEditorModeTools(), &InPreviewScene, TWeakPtr<SEditorViewport>(InViewportWidget))
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

	// 清理旧预览角色（UE 5.8 的 FPreviewScene 不再托管 Actor，直接销毁即可）
	if (AActor* OldActor = PreviewActor.Get())
	{
		PreviewScene->GetWorld()->DestroyActor(OldActor);
		PreviewActor = nullptr;
	}

	TSubclassOf<AActor> PreviewClass = ResolvePreviewCharacterClass(InAsset);
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

		// 手填的是覆盖值；没填就取资产里第一个 PlayMontage 任务的 Montage
		UAnimMontage* Montage = ResolvePreviewMontage(AssetPtr);
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
