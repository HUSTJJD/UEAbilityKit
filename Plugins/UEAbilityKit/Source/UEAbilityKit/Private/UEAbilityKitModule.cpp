#include "UEAbilityKitModule.h"

#include "NativeGameplayTags.h"
#include "UAKAutoCommands.h"

DEFINE_LOG_CATEGORY(LogUAK);

// 原生 Tag 必须定义在 Runtime 模块（客户端与服务端 Tag 必须一致），
// 因此放在这里，再通过 GetUAKTestTag() 提供给测试模块与调试代码。
UE_DEFINE_GAMEPLAY_TAG_STATIC(GUAKTestTag, "UAK.Test.Tag");

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
