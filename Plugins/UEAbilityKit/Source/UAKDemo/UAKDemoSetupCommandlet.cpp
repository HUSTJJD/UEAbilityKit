#if WITH_EDITOR

#include "UAKDemoSetupCommandlet.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityJsonSerializer.h"
#include "UAKAbilityTrack.h"
#include "UAKDemoAbility.h"
#include "UAKDemoCharacter.h"
#include "UAKDemoEffects.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogUAKDemoSetup, Log, All);

namespace
{
	/**
	 * 示例资产放在**插件自己的 Content** 里，插件自己管。
	 *
	 * 插件名 UEAbilityKit 的 Content 挂载点是 /UEAbilityKit/，
	 * 所以资产路径也跟着换：/Game/Demo/... → /UEAbilityKit/Demo/...
	 * 这样插件被拷到任何工程（Lyra 也好、空工程也好）都能一键生成示例，
	 * 不会往宿主工程的 Content 里写东西。
	 */
	const TCHAR* PluginName = TEXT("UEAbilityKit");
	const TCHAR* PluginMount = TEXT("/UEAbilityKit");

	const TCHAR* DemoContentRoot = TEXT("/UEAbilityKit/Demo");
	const TCHAR* AbilityFolder = TEXT("/UEAbilityKit/Demo/Abilities");
	const TCHAR* MapPath = TEXT("/UEAbilityKit/Demo/Maps/L_Demo");

	/** 插件的 Content 目录（磁盘路径）；找不到插件时退回工程目录 */
	FString GetPluginContentDir()
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName))
		{
			return FPaths::ConvertRelativePathToFull(Plugin->GetContentDir());
		}

		UE_LOG(LogUAKDemoSetup, Warning, TEXT("找不到插件 %s，退回工程 Content 目录"), PluginName);
		return FPaths::ProjectContentDir();
	}

	/** 建一个资产（不用 Factory：插件的 Factory 在编辑器模块里，这里拿不到） */
	template <class T>
	T* NewAsset(UObject* Outer, const FString& Name, EObjectFlags ExtraFlags = RF_NoFlags)
	{
		T* Asset = NewObject<T>(Outer, T::StaticClass(), *Name, RF_Public | RF_Standalone | ExtraFlags);
		FAssetRegistryModule::AssetCreated(Asset);
		Asset->MarkPackageDirty();
		return Asset;
	}

	UPackage* NewPackage(const FString& PackagePath)
	{
		UPackage* Package = CreatePackage(*PackagePath);
		if (Package)
		{
			Package->FullyLoad();
		}
		return Package;
	}

	/** 把 GameDefaultMap 写进工程的 Config/DefaultEngine.ini（没有对应节就补一节） */
	bool SetProjectDefaultMap(const FString& InMapPath, FString& OutError)
	{
		const FString IniPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultEngine.ini"));
		const FString Section = TEXT("[/Script/EngineSettings.GameMapsSettings]");
		const FString Entry = FString::Printf(TEXT("GameDefaultMap=%s"), *InMapPath);

		FString Text;
		const bool bExists = FFileHelper::LoadFileToString(Text, *IniPath);

		TArray<FString> Lines;
		if (bExists)
		{
			Text.ParseIntoArrayLines(Lines);
		}

		TArray<FString> Out;
		bool bInSection = false;
		bool bWritten = false;

		for (const FString& Line : Lines)
		{
			const FString Trimmed = Line.TrimStartAndEnd();

			if (Trimmed.StartsWith(TEXT("[")))
			{
				// 离开上一节时还没写，就补在这一节之前
				if (bInSection && !bWritten)
				{
					Out.Add(Entry);
					bWritten = true;
				}

				bInSection = (Trimmed == Section);
				Out.Add(Line);
				continue;
			}

			if (bInSection && !bWritten && Trimmed.StartsWith(TEXT("GameDefaultMap")))
			{
				Out.Add(Entry);
				bWritten = true;
				continue;
			}

			Out.Add(Line);
		}

		if (!bWritten)
		{
			if (!bInSection)
			{
				Out.Add(TEXT(""));
				Out.Add(Section);
			}
			Out.Add(Entry);
		}

		const FString NewText = FString::Join(Out, LINE_TERMINATOR) + LINE_TERMINATOR;
		if (!FFileHelper::SaveStringToFile(NewText, *IniPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("写入失败：%s"), *IniPath);
			return false;
		}

		return true;
	}
}

UUAKDemoSetupCommandlet::UUAKDemoSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UUAKDemoSetupCommandlet::Main(const FString& Params)
{
	UE_LOG(LogUAKDemoSetup, Display, TEXT("=== UAK 示例资产搭建（资产全部落在插件自己的 Content 里）==="));

	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamMap;
	ParseCommandLine(*Params, Tokens, Switches, ParamMap);

	// 默认**不**改宿主的 GameDefaultMap：宿主可能是 Lyra，
	// 把默认地图换成示例关卡会顶掉它自己的流程。需要时显式加 -SetDefaultMap。
	const bool bSetDefaultMap = Switches.Contains(TEXT("SetDefaultMap"));

	// ── 1. 技能资产（Json → 资产）───────────────────────────────────
	TArray<UAKAbilityAsset*> AbilityAssets;
	const int32 Created = CreateAbilityAssets(AbilityAssets);
	UE_LOG(LogUAKDemoSetup, Display, TEXT("技能资产：%d 个"), Created);

	FirstAbility = AbilityAssets.Num() > 0 ? AbilityAssets[0] : nullptr;

	// ── 2. 技能蓝图：指向第一条时间轴 + C++ 的冷却 / 消耗 GE ──────────
	UBlueprint* AbilityBP = CreateBlueprintAsset(UAKDemoAbility::StaticClass(), TEXT("BP_SampleAbility"));
	if (!AbilityBP)
	{
		UE_LOG(LogUAKDemoSetup, Error, TEXT("创建 BP_SampleAbility 失败"));
		return 1;
	}

	UObject* AbilityCDO = AbilityBP->GeneratedClass->GetDefaultObject<UObject>();
	SetObjectProperty(AbilityCDO, TEXT("AbilityAsset"), FirstAbility);
	SetObjectProperty(AbilityCDO, TEXT("SampleCooldownEffect"), UAKDemoGE_Cooldown::StaticClass());
	SetObjectProperty(AbilityCDO, TEXT("SampleCostEffect"), UAKDemoGE_Cost::StaticClass());

	// ── 3. 角色蓝图：Mannequin + 默认技能 + 初始属性 ──────────────────
	UBlueprint* CharacterBP = CreateBlueprintAsset(AAKDemoCharacter::StaticClass(), TEXT("BP_DemoCharacter"));
	if (!CharacterBP)
	{
		UE_LOG(LogUAKDemoSetup, Error, TEXT("创建 BP_DemoCharacter 失败"));
		return 1;
	}

	AActor* CharacterCDO = CharacterBP->GeneratedClass->GetDefaultObject<AActor>();

	if (USkeletalMesh* Mannequin = LoadObject<USkeletalMesh>(nullptr,
		*(FString::Printf(TEXT("%s/AnimationAssets/UE4_Mannequin/Mesh/SK_Mannequin.SK_Mannequin"), PluginMount))))
	{
		if (USkeletalMeshComponent* MeshComp = CharacterCDO->FindComponentByClass<USkeletalMeshComponent>())
		{
			MeshComp->SetSkeletalMesh(Mannequin);
		}
	}
	else
	{
		UE_LOG(LogUAKDemoSetup, Warning, TEXT("找不到 SK_Mannequin（动画资产缺失），角色会没有网格"));
	}

	SetObjectProperty(CharacterCDO, TEXT("DefaultAttributesEffect"), UAKDemoGE_DefaultAttributes::StaticClass());

	// DefaultAbilities 是数组：用 FScriptArrayHelper 加一项
	if (FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(
		CharacterBP->GeneratedClass, TEXT("DefaultAbilities")))
	{
		FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(CharacterCDO));
		Helper.AddValue();

		if (FObjectPropertyBase* Inner = CastField<FObjectPropertyBase>(ArrayProp->Inner))
		{
			Inner->SetObjectPropertyValue(Helper.GetRawPtr(Helper.Num() - 1), AbilityBP->GeneratedClass);
		}
	}

	// ── 4. 关卡：地面 + 光 + 出生点 + 角色 ────────────────────────────
	FKismetEditorUtilities::CompileBlueprint(AbilityBP);
	FKismetEditorUtilities::CompileBlueprint(CharacterBP);

	const int32 LevelResult = CreateDemoLevel(CharacterBP->GeneratedClass);
	if (LevelResult != 0)
	{
		return LevelResult;
	}

	// ── 5. 设为默认地图并保存 ────────────────────────────────────────
	// 直接改工程里的 Config/DefaultEngine.ini：GConfig->Flush 只会写进
	// Saved/Config（本机用户配置），不会落到仓库里——那样"打开工程就落在示例关卡"就不成立了。
	if (bSetDefaultMap)
	{
		FString IniError;
		if (!SetProjectDefaultMap(MapPath, IniError))
		{
			UE_LOG(LogUAKDemoSetup, Warning, TEXT("设置默认地图失败：%s（可在编辑器里手动设置）"), *IniError);
		}
		else
		{
			UE_LOG(LogUAKDemoSetup, Display, TEXT("  ✓ Config/DefaultEngine.ini：GameDefaultMap=%s"), MapPath);
		}
	}
	else
	{
		UE_LOG(LogUAKDemoSetup, Display, TEXT("  · 未指定 -SetDefaultMap：不动宿主的 GameDefaultMap"));
	}

	UEditorLoadingAndSavingUtils::SaveDirtyPackages(/*bSaveMapPackages=*/true, /*bSaveContentPackages=*/true);

	UE_LOG(LogUAKDemoSetup, Display, TEXT("完成：默认地图 = %s。现在可以用编辑器打开并 Play。"), MapPath);
	return 0;
}

int32 UUAKDemoSetupCommandlet::CreateAbilityAssets(TArray<UAKAbilityAsset*>& OutAssets)
{
	// Json 也在插件的 Content 里（Plugins/UEAbilityKit/Content/Demo/Abilities）
	const FString JsonDir = FPaths::Combine(GetPluginContentDir(), TEXT("Demo"), TEXT("Abilities"));

	TArray<FString> JsonFiles;
	IFileManager::Get().FindFiles(JsonFiles, *FPaths::Combine(JsonDir, TEXT("*.json")), true, false);

	for (const FString& FileName : JsonFiles)
	{
		const FString FullPath = FPaths::Combine(JsonDir, FileName);
		const FString AssetName = FPaths::GetBaseFilename(FileName);
		const FString PackagePath = FString::Printf(TEXT("%s/%s"), AbilityFolder, *AssetName);

		UPackage* Package = NewPackage(PackagePath);
		if (!Package)
		{
			UE_LOG(LogUAKDemoSetup, Warning, TEXT("无法创建包：%s"), *PackagePath);
			continue;
		}

		// 同样可反复跑：已存在就复用并重导一次（Json 改了之后重跑命令就能刷新资产）
		UAKAbilityAsset* Asset = FindObject<UAKAbilityAsset>(Package, *AssetName);
		if (!Asset)
		{
			Asset = NewAsset<UAKAbilityAsset>(Package, AssetName);
		}

		if (!UAKAbilityJsonSerializer::ImportFromFile(Asset, FullPath))
		{
			UE_LOG(LogUAKDemoSetup, Warning, TEXT("导入失败：%s"), *FullPath);
			continue;
		}

		// 把导入结果打出来：Json 里的 $type 解析不到类时任务会被静默跳过，
		// 光看"导入成功"根本发现不了——看到"0 个任务"就该去查 $type。
		int32 TaskCount = 0;
		for (const UAKAbilityTrack* Track : Asset->Tracks)
		{
			TaskCount += Track ? Track->Tasks.Num() : 0;
		}

		UE_LOG(LogUAKDemoSetup, Display, TEXT("  ✓ %s ← %s（轨道 %d 条 / 任务 %d 个 / 引导条件 %d 个）"),
			*PackagePath, *FileName, Asset->Tracks.Num(), TaskCount, Asset->ChannelingConditions.Num());
		OutAssets.Add(Asset);
	}

	if (OutAssets.Num() == 0)
	{
		UE_LOG(LogUAKDemoSetup, Warning, TEXT("%s 下没有 Json 技能文件，将创建一条空的技能资产"), *JsonDir);

		UPackage* Package = NewPackage(FString::Printf(TEXT("%s/AB_Demo"), AbilityFolder));
		if (Package)
		{
			OutAssets.Add(NewAsset<UAKAbilityAsset>(Package, TEXT("AB_Demo")));
		}
	}

	return OutAssets.Num();
}

UBlueprint* UUAKDemoSetupCommandlet::CreateBlueprintAsset(UClass* ParentClass, const FString& AssetName)
{
	const FString PackagePath = FString::Printf(TEXT("%s/%s"), DemoContentRoot, *AssetName);
	UPackage* Package = NewPackage(PackagePath);
	if (!Package)
	{
		return nullptr;
	}

	// 命令可以反复跑：已存在就复用。直接 CreateBlueprint 会撞上"同名对象已存在"的 assert
	if (UBlueprint* Existing = FindObject<UBlueprint>(Package, *AssetName))
	{
		UE_LOG(LogUAKDemoSetup, Display, TEXT("  ✓ %s 已存在，复用"), *PackagePath);
		return Existing;
	}

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		ParentClass, Package, FName(*AssetName), EBlueprintType::BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());

	if (Blueprint)
	{
		FAssetRegistryModule::AssetCreated(Blueprint);
		Package->MarkPackageDirty();
		UE_LOG(LogUAKDemoSetup, Display, TEXT("  ✓ %s（父类 %s）"), *PackagePath, *ParentClass->GetName());
	}

	return Blueprint;
}

bool UUAKDemoSetupCommandlet::SetObjectProperty(UObject* CDO, const TCHAR* PropertyName, UObject* Value)
{
	if (!CDO || !Value)
	{
		return false;
	}

	FObjectPropertyBase* Prop = CastField<FObjectPropertyBase>(
		FindFProperty<FProperty>(CDO->GetClass(), PropertyName));

	if (!Prop)
	{
		UE_LOG(LogUAKDemoSetup, Warning, TEXT("字段不存在：%s.%s"), *CDO->GetClass()->GetName(), PropertyName);
		return false;
	}

	Prop->SetObjectPropertyValue(Prop->ContainerPtrToValuePtr<void>(CDO), Value);
	return true;
}

int32 UUAKDemoSetupCommandlet::CreateDemoLevel(UClass* CharacterClass)
{
	UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(/*bSaveExistingMap=*/false);
	if (!World)
	{
		UE_LOG(LogUAKDemoSetup, Error, TEXT("无法创建空白关卡"));
		return 1;
	}

	// 地面
	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(FVector(0.f, 0.f, -50.f), FRotator::ZeroRotator);
		if (Floor)
		{
			Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
			Floor->SetActorScale3D(FVector(50.f, 50.f, 1.f));
		}
	}

	ADirectionalLight* Light = World->SpawnActor<ADirectionalLight>(FVector(0.f, 0.f, 500.f), FRotator(-50.f, 0.f, 0.f));
	if (Light)
	{
		Light->SetActorLabel(TEXT("DemoLight"));
	}

	World->SpawnActor<APlayerStart>(FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator);

	// 直接放一个角色并让它自动接管 Player0，省掉再配一个 GameMode
	if (CharacterClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// 用 FTransform 重载：直接传 (UClass*, FVector, FRotator, Params) 会撞上模板重载导致歧义
		const FTransform SpawnTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 120.f));

		if (AAKDemoCharacter* Character = Cast<AAKDemoCharacter>(
			World->SpawnActor<AActor>(CharacterClass, SpawnTransform, SpawnParams)))
		{
			Character->AutoPossessPlayer = EAutoReceiveInput::Player0;
			Character->SetActorLabel(TEXT("DemoCharacter"));
			Character->MarkPackageDirty();
		}
	}

	if (!FEditorFileUtils::SaveMap(World, MapPath))
	{
		UE_LOG(LogUAKDemoSetup, Error, TEXT("保存关卡失败：%s"), MapPath);
		return 1;
	}

	UE_LOG(LogUAKDemoSetup, Display, TEXT("  ✓ 关卡 %s"), MapPath);
	return 0;
}

#endif // WITH_EDITOR
