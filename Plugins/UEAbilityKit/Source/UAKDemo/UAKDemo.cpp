#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * 游戏模块入口点。
 *
 * 这个文件看起来"什么都没做"，但**必须有**：
 * UE 加载 UnrealEditor-UAKDemo.dll 后会查找并调用 InitializeModule 导出函数，
 * 没有它就会报
 *   "The game module 'UAKDemo' could not be successfully initialized after it was loaded."
 * 然后引擎直接退出。
 */
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, UAKDemo, "UAKDemo");
