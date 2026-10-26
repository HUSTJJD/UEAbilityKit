#include "SUAKPreviewViewport.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityTask_PlayMontage.h"
#include "UAKPreviewRunner.h"
#include "UAKSettings.h"

#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameMapsSettings.h"
#include "UObject/UObjectGlobals.h"

#include "ClassViewerModule.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MenuBuilder.h"
#include "ScopedTransaction.h"
#include "Styling/SlateIcon.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
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

		// 宿主工程显式配置（Config/DefaultUEAbilityKit.ini）：配一次，全工程的技能资产通用。
		// 它排在"按 GameMode 推断"之前 —— 推断出来的常常是 C++ 基类（Lyra 下是
		// ALyraCharacter，没配网格），看得见胶囊看不见人。
		if (const UUAKSettings* Settings = GetDefault<UUAKSettings>())
		{
			if (UClass* Configured = Settings->DefaultPreviewCharacterClass.TryLoadClass<AActor>())
			{
				return Configured;
			}
		}

		// 从项目设置推断：GlobalDefaultGameMode → DefaultPawnClass。
		// 那个 UPROPERTY 是 private，只能走静态访问器拿路径字符串。
		FString GameModePath = UGameMapsSettings::GetGlobalDefaultGameMode();
		if (!GameModePath.IsEmpty())
		{
			// 值可能是别名（GameModeClassAliases），解析一次再加载
			GameModePath = UGameMapsSettings::GetGameModeForName(GameModePath);

			if (UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, *GameModePath))
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

		// 不再拿插件随包的示例角色兜底：那会让人预览到一只根本不会出场、骨骼与 Montage
		// 还对不上的角色，比空白更有误导性。拿不到就说清去哪儿配。
		UE_LOG(LogTemp, Verbose,
			TEXT("[UAK] 没有可用的预览角色：填资产上的 PreviewCharacterClass，或在 Config/DefaultUEAbilityKit.ini 里设 DefaultPreviewCharacterClass。"));
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

	// 真跑一次施放时由执行器推进：跑到资产结束即停（bLooping 时自动重开一次）
	if (Runner.IsValid() && Runner->IsRunning())
	{
		Runner->Tick(DeltaSeconds);
		if (!Runner->IsRunning())
		{
			bPlaying = false;
		}
		return;
	}

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

	// 旧角色马上要被销毁：先结束这次施放，别让执行器握着一只已销毁的 Actor
	if (Runner.IsValid())
	{
		Runner->Reset();
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

bool FAKPreviewViewportClient::PlayPreviewAbility(UAKAbilityAsset* InAsset)
{
	if (!PreviewScene || !InAsset)
	{
		return false;
	}

	// 每次施放都从一只干净的角色开始：属性、GE、Cue 全部归零，省得写回滚逻辑。
	RefreshPreviewActor(InAsset);

	AActor* Actor = PreviewActor.Get();
	if (!Actor)
	{
		return false;
	}

	if (!Runner.IsValid())
	{
		Runner = TStrongObjectPtr<UUAKPreviewRunner>(NewObject<UUAKPreviewRunner>());
	}

	const bool bStarted = Runner->Start(Actor, InAsset, bLooping);
	bPlaying = bStarted;
	PreviewTime = 0.f;
	return bStarted;
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

	if (Runner.IsValid())
	{
		Runner->Stop();
	}

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
	Asset = InAsset;

	if (ViewportClient.IsValid())
	{
		ViewportClient->RefreshPreviewActor(InAsset);
	}
}

FText SUAKEditorViewport::GetPreviewCharacterLabel() const
{
	const UAKAbilityAsset* AssetPtr = Asset.Get();

	if (AssetPtr && !AssetPtr->PreviewCharacterClass.IsNull())
	{
		return FText::FromString(AssetPtr->PreviewCharacterClass.GetAssetName());
	}

	// 资产没指定时把实际会用的那只显示出来，省得猜"留空"会发生什么
	if (UClass* Fallback = ResolvePreviewCharacterClass(AssetPtr))
	{
		return FText::Format(LOCTEXT("PreviewCharDefault", "默认（{0}）"), FText::FromName(Fallback->GetFName()));
	}

	return LOCTEXT("PreviewCharNone", "（未配置）");
}

TSharedRef<SWidget> SUAKEditorViewport::MakePreviewCharacterMenu()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("PreviewCharFollowDefault", "跟随工程默认"),
		LOCTEXT("PreviewCharFollowDefaultTip", "清空本资产的指定，用工程默认的那只角色"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SUAKEditorViewport::OnPreviewCharacterPicked, (UClass*)nullptr)));

	MenuBuilder.AddSeparator();

	// 挑一只角色：只看可放蓝图（含未加载的），选完直接写进资产
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::ListView;
	Options.bShowObjectRootClass = false;
	Options.bShowUnloadedBlueprints = true;
	Options.bIsBlueprintBaseOnly = true;

	MenuBuilder.AddWidget(
		ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SUAKEditorViewport::OnPreviewCharacterPicked)),
		FText::GetEmpty(),
		/*bNoIndent=*/true);

	return MenuBuilder.MakeWidget();
}

void SUAKEditorViewport::OnPreviewCharacterPicked(UClass* PickedClass)
{
	UAKAbilityAsset* AssetPtr = Asset.Get();
	if (!AssetPtr)
	{
		return;
	}

	// 写进资产：每个技能自己记住用哪只角色预览，随资产一起保存，
	// 别人打开这个技能看到的就是同一只。传 nullptr = 清掉指定，回到工程默认。
	FScopedTransaction Transaction(LOCTEXT("SetPreviewCharacter", "设置预览角色"));
	AssetPtr->Modify();
	AssetPtr->PreviewCharacterClass = PickedClass;
	AssetPtr->MarkPackageDirty();

	OnAssetChanged(AssetPtr);
}

FReply SUAKEditorViewport::OnPlayClicked()
{
	if (UAKAbilityAsset* AssetPtr = Asset.Get())
	{
		if (!ViewportClient.IsValid())
		{
			return FReply::Handled();
		}

		// 优先真跑一次施放：Montage / Cue / 位移都出来，生命周期就是技能本身。
		// 跑不起来（没角色或没资产）才退回只播一个 Montage。
		if (!ViewportClient->PlayPreviewAbility(AssetPtr))
		{
			if (UAnimMontage* Montage = ResolvePreviewMontage(AssetPtr))
			{
				ViewportClient->PlayPreviewMontage(Montage);
			}
			else
			{
				ViewportClient->SetPlaying(true);
			}
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
