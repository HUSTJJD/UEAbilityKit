#include "UEAbilityKitModule.h"

#include "NativeGameplayTags.h"
#include "UAKAutoCommands.h"

DEFINE_LOG_CATEGORY(LogUAK);

// 原生 Tag 必须定义在 Runtime 模块（客户端与服务端 Tag 必须一致），
// 因此放在这里，再通过 GetUAKTestTag() 提供给测试模块与调试代码。
UE_DEFINE_GAMEPLAY_TAG_STATIC(GUAKTestTag, "UAK.Test.Tag");

// 教程脚本（Tutorials/*.json）里写死的示例 Tag。
//
// 为什么必须由代码注册：教程是"能被真实执行"的文档——UAK.Tutorial.Run 会把脚本点一遍，
// UAK.Tutorial.Scripts 会在数据层模拟一遍并断言每一步的校验数。两者都要求脚本里
// 写的 Tag 真实存在：GameplayTag 只能引用已注册的 Tag，未注册的 Tag 在
// ImportText 时会被丢掉（容器仍是空的），于是"加一个 Ability.Casting"就教了一件
// 做不到的事，而且校验数永远对不上。
//
// 用原生注册而不是 .ini：换宿主工程不用再配一次，也不会出现"配了但没生效"。
UE_DEFINE_GAMEPLAY_TAG_STATIC(GUAKTutorialTagCasting, "Ability.Casting");
UE_DEFINE_GAMEPLAY_TAG_STATIC(GUAKTutorialTagWindup, "Ability.Windup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(GUAKTutorialCueHit, "GameplayCue.Demo.Hit");

FGameplayTag GetUAKTestTag()
{
	return GUAKTestTag;
}

void FUEAbilityKitModule::StartupModule()
{
	// UAK.Auto.*：编辑器 / Standalone Game / Dedicated Server 都能用的自动化桥接命令。
	// 放在 Runtime 模块，是因为测试模块（Editor 类型）在 -game / -server 进程里不一定加载。
	FUAKAutoCommands::Register();

	UE_LOG(LogUAK, Log, TEXT("UEAbilityKit (UAK) loaded."));
}

void FUEAbilityKitModule::ShutdownModule()
{
	FUAKAutoCommands::Unregister();

	UE_LOG(LogUAK, Log, TEXT("UEAbilityKit (UAK) unloaded."));
}

IMPLEMENT_MODULE(FUEAbilityKitModule, UEAbilityKit)
