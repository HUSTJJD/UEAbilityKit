#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "UAKDemoSetupCommandlet.generated.h"

class UAKAbilityAsset;
class UBlueprint;

/**
 * 一键搭好示例工程：资产 + 蓝图 + 关卡，全部可命令行生成。
 *
 * 用法：
 *   UnrealEditor-Cmd.exe <Project> -run=UAKDemoSetup [-unattended] [-nosplash]
 *
 * 为什么要有它：
 * 示例工程的价值在于"clone 下来就能跑"。但 BP、关卡、技能资产都是二进制，
 * 写进仓库既不能 diff 也不该由手工点击产生——所以这里把它们变成**一条命令**，
 * 和 UAKValidate（CI 校验）配成一对：一个负责产出，一个负责守住。
 *
 * 放在示例模块（而不是插件的编辑器模块）里，是因为依赖方向：
 * 插件不该知道示例工程的存在，反过来才是合理的。
 */
UCLASS()
class UUAKDemoSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UUAKDemoSetupCommandlet();

	virtual int32 Main(const FString& Params) override;

private:
	/** 从 Content/Demo/Abilities/*.json 建出技能资产 */
	int32 CreateAbilityAssets(TArray<UAKAbilityAsset*>& OutAssets);

	/** 建一个蓝图并取它的 CDO，便于用反射填默认值 */
	UBlueprint* CreateBlueprintAsset(UClass* ParentClass, const FString& AssetName);

	/** 用反射给 CDO 的某个对象型字段赋值（TSubclassOf / UObject* 都走这条） */
	static bool SetObjectProperty(UObject* CDO, const TCHAR* PropertyName, UObject* Value);

	int32 CreateDemoLevel(UClass* CharacterClass);

	UAKAbilityAsset* FirstAbility = nullptr;
};

#endif // WITH_EDITOR
