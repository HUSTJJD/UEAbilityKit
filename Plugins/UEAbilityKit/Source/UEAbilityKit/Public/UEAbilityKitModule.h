#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUAK, Log, All);

/**
 * 供自动化测试与调试使用的 Tag（UAK.Test.Tag）。
 *
 * 为什么不直接在测试模块里定义原生 Tag：原生 Tag 的模块类型必须是
 * Runtime / RuntimeAndProgram（客户端与服务端的 Tag 必须一致），
 * 定义在 Editor 类型的测试模块里会被引擎拒绝注册。
 * 也不能直接 extern 一个 FNativeGameplayTag 变量跨模块——那需要导出变量，
 * 用函数取值是最省事且最不易踩坑的方式。
 */
UEABILITYKIT_API FGameplayTag GetUAKTestTag();

class FUEAbilityKitModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
