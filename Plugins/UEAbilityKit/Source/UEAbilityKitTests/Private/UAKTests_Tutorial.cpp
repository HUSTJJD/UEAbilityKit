#include "CoreMinimal.h"

#include "UAKTestCommon.h"

#include "UAKAbilityAsset.h"
#include "UAKAbilityJsonSerializer.h"
#include "UAKAbilityTaskBase.h"
#include "UAKAbilityTrack.h"
#include "UAKTypes.h"

#include "UAKTutorialAutomation.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 教程脚本的静态可执行性检查。
 *
 * 为什么要有这个用例：教程是"文档"，但它的每一步同时是"脚本"。
 * 脚本会腐化——改了任务的属性名、删了某个任务类型、教程前后步骤对不上，
 * 文档就会教人做一件做不到的事，而且**没人会发现**，直到有读者照着点失败。
 *
 * 这里不打开任何编辑器窗口（不依赖 UI / RHI），只用数据层模拟一遍：
 * 顺序建轨道 → 建任务 → 改属性 → 跑校验，
 * 断言每一步引用的东西都真实存在。真正带截图的录制由 UAK.Tutorial.Run 做。
 */
namespace
{
	/** 模拟执行时的资产状态（只记"有几条轨道、名字叫什么"） */
	struct FSimState
	{
		UAKAbilityAsset* Asset = nullptr;
		TMap<FString, int32> TrackIndexByName;
	};

	FString ArgStr(const FUAKTutorialStep& Step, const TCHAR* Key)
	{
		FString Value;
		return Step.Args.IsValid() && Step.Args->TryGetStringField(Key, Value) ? Value : FString();
	}

	int32 ArgInt(const FUAKTutorialStep& Step, const TCHAR* Key, int32 Default)
	{
		int32 Value = 0;
		return (Step.Args.IsValid() && Step.Args->TryGetNumberField(Key, Value)) ? Value : Default;
	}

	/** 与运行时同一套解析规则：track 既能写下标也能写名字 */
	int32 ResolveTrackIndex(const FSimState& Sim, const FUAKTutorialStep& Step)
	{
		const FString Name = ArgStr(Step, TEXT("track"));
		if (Name.IsEmpty())
		{
			return ArgInt(Step, TEXT("trackIndex"), 0);
		}
		if (FCString::IsNumeric(*Name))
		{
			return FCString::Atoi(*Name);
		}
		const int32* Found = Sim.TrackIndexByName.Find(Name);
		return Found ? *Found : INDEX_NONE;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAKTutorialScriptsTest, "UAK.Tutorial.Scripts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUAKTutorialScriptsTest::RunTest(const FString& Parameters)
{
	const FString ScriptDir = FUAKTutorialAutomation::GetScriptDir();
	if (!IFileManager::Get().DirectoryExists(*ScriptDir))
	{
		AddInfo(FString::Printf(TEXT("没有教程脚本目录 %s（只装插件、没带教程的工程是正常的）"), *ScriptDir));
		return true;
	}

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(ScriptDir / TEXT("*.json")), true, false);
	Files.Sort();
	TestTrue(TEXT("教程目录里至少有一条教程"), Files.Num() > 0);

	int32 CheckedSteps = 0;

	for (const FString& File : Files)
	{
		FUAKTutorialScript Script;
		FString Error;
		const bool bLoaded = FUAKTutorialScript::LoadFromFile(ScriptDir / File, Script, Error);
		TestTrue(FString::Printf(TEXT("%s 能解析：%s"), *File, *Error), bLoaded);
		if (!bLoaded)
		{
			continue;
		}

		TestFalse(TEXT("教程 id 不为空"), Script.Id.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s 至少有一个步骤"), *Script.Id), Script.Steps.Num() > 0);

		FSimState Sim;
		Sim.Asset = UAKTest::NewAsset(Script.Asset.Length);

		for (const FUAKTutorialStep& Step : Script.Steps)
		{
			++CheckedSteps;
			const FString Where = FString::Printf(TEXT("%s / %s"), *Script.Id, *Step.Id);

			// 教学三要素缺一个，生成的文档就"没在教"
			TestFalse(FString::Printf(TEXT("%s 有标题"), *Where), Step.Title.IsEmpty());
			TestFalse(FString::Printf(TEXT("%s 有讲解（say）"), *Where), Step.Say.IsEmpty());

			switch (Step.Action)
			{
			case EUAKTutorialAction::AddTrack:
			{
				UAKAbilityTrack* Track = NewObject<UAKAbilityTrack>(Sim.Asset);
				Track->TrackName = FName(*ArgStr(Step, TEXT("name")));
				Track->TrackType = static_cast<EAKTrackType>(ArgInt(Step, TEXT("type"), 0));
				const int32 Index = Sim.Asset->Tracks.Add(Track);
				Sim.TrackIndexByName.Add(Track->TrackName.ToString(), Index);
				break;
			}

			case EUAKTutorialAction::AddTask:
			{
				const int32 Index = ResolveTrackIndex(Sim, Step);
				TestTrue(FString::Printf(TEXT("%s 引用的轨道存在"), *Where), Sim.Asset->Tracks.IsValidIndex(Index));
				if (!Sim.Asset->Tracks.IsValidIndex(Index))
				{
					break;
				}

				const FString TypeName = ArgStr(Step, TEXT("type"));
				UAKAbilityTaskBase* Task = UAKAbilityJsonSerializer::CreateTaskByTypeName(
					Sim.Asset->Tracks[Index], TypeName);
				TestTrue(FString::Printf(TEXT("%s 的任务类型 %s 存在"), *Where, *TypeName), Task != nullptr);
				if (!Task)
				{
					break;
				}

				// props 里的每个键都必须是真实属性，否则教程教了一个改不动的参数
				const TSharedPtr<FJsonObject>* Props = nullptr;
				if (Step.Args->TryGetObjectField(TEXT("props"), Props) && Props && Props->IsValid())
				{
					for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Props)->Values)
					{
						TestTrue(FString::Printf(TEXT("%s 上能找到属性 %s"), *Where, *Pair.Key),
							FindFProperty<FProperty>(Task->GetClass(), *Pair.Key) != nullptr);
					}
				}

				Sim.Asset->Tracks[Index]->Tasks.Add(Task);
				break;
			}

			case EUAKTutorialAction::SetTaskProp:
			{
				const int32 TrackIndex = ResolveTrackIndex(Sim, Step);
				const int32 TaskIndex = ArgInt(Step, TEXT("task"), ArgInt(Step, TEXT("taskIndex"), 0));
				TestTrue(FString::Printf(TEXT("%s 引用的轨道存在"), *Where),
					Sim.Asset->Tracks.IsValidIndex(TrackIndex));
				if (!Sim.Asset->Tracks.IsValidIndex(TrackIndex))
				{
					break;
				}

				const TArray<TObjectPtr<UAKAbilityTaskBase>>& Tasks = Sim.Asset->Tracks[TrackIndex]->Tasks;
				TestTrue(FString::Printf(TEXT("%s 引用的任务存在"), *Where), Tasks.IsValidIndex(TaskIndex));
				if (!Tasks.IsValidIndex(TaskIndex) || !Tasks[TaskIndex])
				{
					break;
				}

				const FString Property = ArgStr(Step, TEXT("property"));
				TestTrue(FString::Printf(TEXT("%s 的属性 %s 存在"), *Where, *Property),
					!Property.IsEmpty()
					&& FindFProperty<FProperty>(Tasks[TaskIndex]->GetClass(), *Property) != nullptr);
				break;
			}

			case EUAKTutorialAction::SetAssetProp:
			{
				const FString Property = ArgStr(Step, TEXT("property"));
				TestTrue(FString::Printf(TEXT("%s 的资产属性 %s 存在"), *Where, *Property),
					!Property.IsEmpty()
					&& FindFProperty<FProperty>(UAKAbilityAsset::StaticClass(), *Property) != nullptr);
				break;
			}

			case EUAKTutorialAction::Validate:
			{
				FAKValidationResult Result;
				Sim.Asset->Validate(Result);
				TestEqual(FString::Printf(TEXT("%s 的错误数符合脚本声明"), *Where),
					Result.NumErrors(), Step.ExpectErrors);
				break;
			}

			default:
				break;
			}
		}
	}

	AddInfo(FString::Printf(TEXT("共检查 %d 条教程脚本、%d 个步骤"), Files.Num(), CheckedSteps));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
