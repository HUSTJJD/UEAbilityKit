#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * 测试模块的入口点。
 *
 * 用例本身靠 IMPLEMENT_SIMPLE_AUTOMATION_TEST 静态注册，模块启动时不做事，
 * 但入口点**必须有**：否则引擎加载 DLL 后找不到 InitializeModule，会报
 *   "Plugin 'UEAbilityKit' failed to load because module 'UEAbilityKitTests'
 *    could not be initialized successfully after it was loaded."
 * 并且会导致整个插件加载失败（连带 Runtime / Editor 模块都不可用）。
 */
IMPLEMENT_MODULE(FDefaultModuleImpl, UEAbilityKitTests);
