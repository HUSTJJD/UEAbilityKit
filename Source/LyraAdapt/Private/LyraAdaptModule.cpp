#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * LyraAdapt：UAK 适配 Lyra 的那一层。
 *
 * 为什么单独一个模块：
 * 产品插件 Plugins/UEAbilityKit 必须**完整且内聚**——它只认识 GAS，不认识任何具体游戏。
 * 凡是"必须提到 Lyra"的代码（Lyra 的输入、Lyra 的武器实例、Lyra 的相机模式），
 * 都放这里，不进插件。这样插件能干净地服务任何游戏，Lyra 只是其中一个示例宿主。
 *
 * 依赖方向：LyraAdapt → { LyraGame, UEAbilityKit }（单向，两边都不知道它）
 */
IMPLEMENT_MODULE(FDefaultModuleImpl, LyraAdapt);
