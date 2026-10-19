#include "UAKValidateCommandlet.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityJsonSerializer.h"
#include "UAKTypes.h"
#include "UEAbilityKitModule.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogUAKValidate, Log, All);

UAKValidateCommandlet::UAKValidateCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UAKValidateCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamsMap;
	ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

	const FString Path = ParamsMap.Contains(TEXT("path")) ? ParamsMap[TEXT("path")] : TEXT("/Game");
	const bool bStrict = Switches.Contains(TEXT("strict"));
	const bool bExportJson = Switches.Contains(TEXT("json"));

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.WaitForCompletion();

	FARFilter Filter;
	Filter.ClassPaths.Add(UAKAbilityAsset::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetDatas;
	Registry.GetAssets(Filter, AssetDatas);

	UE_LOG(LogUAKValidate, Display, TEXT("[UAK] 校验路径 %s，共 %d 个技能资产"), *Path, AssetDatas.Num());

	int32 TotalErrors = 0;
	int32 TotalWarnings = 0;

	const FString ExportDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UAK"));

	for (const FAssetData& AssetData : AssetDatas)
	{
		UAKAbilityAsset* Asset = Cast<UAKAbilityAsset>(AssetData.GetAsset());
		if (!Asset)
		{
			continue;
		}

		FAKValidationResult Result;
		Asset->Validate(Result);

		TotalErrors += Result.NumErrors();
		TotalWarnings += Result.NumWarnings();

		for (const FAKValidationIssue& Issue : Result.Issues)
		{
			const TCHAR* Severity =
				(Issue.Severity == EAKValidationSeverity::Error) ? TEXT("错误") :
				(Issue.Severity == EAKValidationSeverity::Warning) ? TEXT("警告") : TEXT("提示");

			UE_LOG(LogUAKValidate, Display, TEXT("[%s] %s: %s"),
				Severity, *Asset->GetName(), *Issue.Message.ToString());
		}

		if (bExportJson)
		{
			IFileManager::Get().MakeDirectory(*ExportDir, true);
			const FString FilePath = FPaths::Combine(ExportDir, Asset->GetName() + TEXT(".json"));
			UAKAbilityJsonSerializer::ExportToFile(Asset, FilePath);
		}
	}

	UE_LOG(LogUAKValidate, Display, TEXT("[UAK] 校验完成：%d 错误 · %d 警告"), TotalErrors, TotalWarnings);

	if (bStrict && TotalErrors > 0)
	{
		return 1;
	}
	return 0;
}
