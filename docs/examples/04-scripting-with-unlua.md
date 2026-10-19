# 脚本方案：UnLuaNova（git submodule）

## 1. 为什么是 submodule

UnLuaNova 有自己的仓库与版本节奏，不应该被复制进 UAK：

- 保持**单一来源**：升级 UnLuaNova 只需移动 submodule 指针
- UAK 的仓库体积不被第三方源码撑大
- 示例工程里 Lua 与 C++ 的边界一眼可见

```bash
git clone --recursive https://github.com/HUSTJJD/UEAbilityKit.git
# 已经克隆过的：
git submodule update --init --recursive
```

## 2. 目录位置

`UnLuaNova` 仓库本身是一个 UE 工程（内层的插件在 `Plugins/UnLuaNova/`），因此：

```
Plugins/UnLuaNova/                          ← submodule 根（UnLuaNova 工程）
└─ Plugins/UnLuaNova/UnLuaNova.uplugin      ← 真正的插件
```

为了让 UE 稳定发现它，`UAKDemo.uproject` 里加了：

```json
"AdditionalPluginDirectories": [ "Plugins/UnLuaNova/Plugins" ]
```

若你的引擎版本不支持该字段，把内层 `Plugins/UnLuaNova` 复制到工程的 `Plugins/` 下即可（二选一，不要同时存在以免重复注册）。

## 3. 依赖声明

只有**示例模块**依赖脚本系统，UAK 插件本身不依赖任何脚本：

```csharp
// Source/UAKDemo/UAKDemo.Build.cs
PublicDependencyModuleNames.AddRange(new[] {
    "Core", "CoreUObject", "Engine",
    "GameplayAbilities", "GameplayTags",
    "UEAbilityKit",
    "UnLuaNova"     // 仅示例工程使用
});
```

`Plugins/UEAbilityKit/UEAbilityKit.Build.cs` 里**没有** UnLuaNova——这是有意的。

## 4. 分工：核心 C++，扩展用 Lua

| 层 | 用什么 | 原因 |
|---|---|---|
| 执行器、任务基类、编辑器、序列化 | **C++** | 需要强类型、反射、校验、可静态分析 |
| 内置任务（GE / Montage / Cue / 查询 / 投射物…） | **C++** | 通用能力，所有项目共享 |
| 输入绑定、UI 联动、一次性玩法逻辑 | **Lua** | 迭代快、项目差异大 |
| 自定义任务 | **Lua（通过 C++ 子类转发）** | 项目特有行为，不值得进框架 |
| 自定义引导条件（如"按住按键"） | **Lua（通过 `UAKChanneling_CustomCondition` 转发）** | UAK 刻意不内置输入系统 |

示例：`Content/Script/UAKDemoCharacter.lua`

```lua
function M:IsChannelingInputHeld()
    -- C++ 侧的 UAKChanneling_CustomCondition 通过 UnLuaNova 调到这里
    return true
end
```

这样"输入松手即打断蓄力"这个**项目特有**的语义就留在脚本里，
而 UAK 只提供一个类型安全的扩展点——框架不会被任何一种输入系统绑死。

## 5. 角色侧绑定

`AAKDemoCharacter` 实现 `IUnLuaNovaInterface`：

```cpp
FString AAKDemoCharacter::GetModuleName_Implementation() const
{
    return TEXT("UAKDemoCharacter");   // → Content/Script/UAKDemoCharacter.lua
}
```

## 6. 不使用脚本时的取舍

示例模块现在硬依赖 `UnLuaNova`。如果你只想编译最小示例：

- 删除 `Content/Script/`
- 从 `UAKDemo.Build.cs` 移除 `"UnLuaNova"`
- `AAKDemoCharacter` 去掉 `IUnLuaNovaInterface` 与 `GetModuleName_Implementation`
- `UAKDemo.uproject` 里把 `UnLuaNova` 设为 `false`

UAK 插件本身不受任何影响。
