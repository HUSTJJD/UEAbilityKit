# 脚本集成：UnLuaNova

> 定位：本文是 **UAK ↔ 脚本方案** 的契约，不含 UnLuaNova 自身用法（那部分在[它的文档](https://hustjjd.github.io/UnLuaNova/)）。
> 操作层面的步骤（目录、依赖、打包、后端、排障）见 [examples/04-scripting-with-unlua.md](../examples/04-scripting-with-unlua.md)。

## 1. 结论（先说定稿的部分）

| 主题 | 结论 |
|---|---|
| 立场 | **UAK 是纯 C++ 的产品插件，不依赖、不内置、不优先推荐任何脚本系统** |
| 脚本的角色 | 只承担**项目特有**部分：输入语义、UI 联动、一次性玩法逻辑、自定义引导条件 |
| 依赖方向 | 单向：`UAKDemo`（示例模块）→ `UnLuaNova`；`UEAbilityKit`（核心）**一行都不引用** |
| 可分发性 | `UEAbilityKit.uplugin` 里的 UnLuaNova 引用必须是 `Optional` —— 否则插件无法独立分发 |
| 扩展点 | 只有两个：**数据驱动绑定**（ini 映射 / 绑定组件 / `IUnLuaNovaInterface`，对象级）、引导条件对象（脚本**喂状态**、C++ 判定） |
| 集成自检 | `Tools/unluanova.ps1 setup` / `doctor`：子模块、后端三处一致性、打包 stage、扩展插件 |
| 打包 | 脚本不是资产，必须显式 stage：工程 `Content/Script` + 插件运行时模块目录 |
| 后端 | 编译期选择（Luau 默认），ini 与二进制不得分叉 |

## 2. 为什么不让脚本进核心

1. **校验与可 diff 是 UAK 的立身之本**：Task 是强类型 `UObject` + `USTRUCT`，才能做到
   `Validate()` 自校验、Json 往返、CI 门禁。把 Task 换成字符串脚本，这三样一起失效
   （见 [01-design-principles.md](01-design-principles.md) §3「不做字符串脚本」）。
2. **可独立分发**：把 `Plugins/UEAbilityKit` 拷进任何项目就能用，是文档里承诺的接入方式。
   核心一旦依赖脚本插件，这条承诺立刻变成"先去装一个 Lua 方案"。
3. **宿主不可预测**：本工程的宿主是 Lyra，而 Lyra 不需要 Lua。脚本只服务"要它的人"。

反过来说：**不该进框架的，要留出口子**。所以 UAK 只提供类型安全的扩展点，
不提供"Lua 任务"这种把脚本语义钉进时间轴的东西 —— 项目要自定义任务，走 C++ 子类；
要自定义条件，走 `UAKChanneling_CustomCondition`。

## 3. 所有权边界（放错位置就是脏）

| 内容 | 归属 | 判据 |
|---|---|---|
| 执行器 / Task 基类 / 编辑器 / 序列化 / Trace | `UEAbilityKit` 模块 | 只认识 GAS |
| 示例角色 + Lua 绑定（`AAKDemoCharacter`） | `UAKDemo` 模块（随插件分发） | 是示例，不是框架 |
| Lyra 的输入条件（`ULyraAdaptInputCondition`） | `Source/LyraAdapt/` | 提到了 Lyra |
| `Content/Script/**` | 工程 | 项目脚本 |
| 插件运行时 Lua 模块（`UnLuaNova.StringEx` 等） | UnLuaNova 插件 | 上游发布 |

判据只有一条：**这段代码必须提到某个具体宿主 / 具体脚本方案吗？** 是 → 挪出去。

## 4. 两个扩展点

### 4.1 对象绑定（数据驱动，默认）

绑定粒度是**对象**（一个 Actor 一个 Lua 模块），不是"全局脚本入口"。
`ULuaModuleLocator_ByBinding` 是默认定位器，按优先级提供三种绑法：

| 方式 | 写法 | 为什么要它 |
|---|---|---|
| **ini 映射**（示例用这个） | `DefaultUnLuaNova.ini` 的 `ClassModuleBindings` | **UE 侧一行代码都不用改**；这是"示例模块对脚本零依赖"的前提 |
| **绑定组件** | 蓝图 Actor 上加 `UUnLuaNovaBindingComponent`，填模块名 | 纯蓝图团队也能用，按资产逐个配 |
| **实现 `IUnLuaNovaInterface`** | 覆写 `GetModuleName()` | 老项目迁移、模块名要按实例算 |

```ini
; Config/DefaultUnLuaNova.ini
[/Script/UnLuaNova.UnLuaNovaSettings]
+ClassModuleBindings=(Class="/Script/UAKDemo.AKDemoCharacter",ModuleName="UAKDemoCharacter")
```

两条硬约束（写扩展点时别踩）：

- **不是"任何对象都会被绑"**：`UUnLuaNovaSettings::bDataDrivenBinding` 默认 false
  （配置项，不是代码常量）。关着时只有实现了接口或走动态绑定的对象会被绑，
  不会让用户在不知情时被绑上脚本
- **匹配会沿继承链向上找**（绑基类即覆盖蓝图子类），但**不解析 Class 资产**：
  `TryBind` 是在 UObject 创建过程中调的，那里 `LoadObject` 是自找麻烦

其余约定不变：

- 脚本能接到的都是 C++ 显式暴露的 `UFUNCTION` / `UPROPERTY`（`BlueprintImplementableEvent` 由脚本实现）
- **输入绑定留在 C++**，脚本只决定"按下去做什么"：绑 `InputComponent` / Enhanced Input 的 API 不是 UFUNCTION，脚本调不到

### 4.2 引导条件：脚本喂状态，C++ 做判定

引导（Channeling）的维持条件由项目回答，UAK 不内置输入系统（见 [19-channeling.md](19-channeling.md)）。
扩展点是**条件对象**（`UAKChannelingCondition` 体系，含可覆写的 `UAKChanneling_CustomCondition`），
不是"Lua 钩子" —— 判定逻辑留在 C++，脚本只负责喂状态：

```
Lua: OnAbilityInput → SetChannelingInputHeld(Slot, bPressed)   ← 写状态（脚本）
C++: UAKDemoChanneling_InputHeld::IsSatisfied → Character->IsChannelingInputHeld(Slot)  ← 每帧判定
```

同一个扩展点在两种宿主下的实现：

| 宿主 | 状态来源 | 条件对象 |
|---|---|---|
| UAKDemo（示例模块） | Lua `SetChannelingInputHeld` | `UAKDemoChanneling_InputHeld`（C++） |
| Lyra（`Source/LyraAdapt/`） | Enhanced Input 的 Pressed/Released | `ULyraAdaptInputCondition`（C++ 子类） |

两条都没有把脚本绑进判定路径：去掉脚本时条件默认成立（示例不会因此永远打断），
换成 C++ 宿主实现也不用碰 Lua。**脚本与 C++ 在这里是平等的两种状态来源。**

## 5. 依赖与打包（容易错的两处）

### 5.1 `Optional` 不是可选项

```json
// UEAbilityKit.uplugin
{ "Name": "UnLuaNova", "Enabled": true, "Optional": true }
```

引擎语义（`PluginManager.cpp`）：引用了找不到的插件时，`Optional` 会让它退成一条 Verbose 日志，
否则是加载错误。也就是说 **没有 `Optional`，UAK 就不能被单独拷进别的项目**。

### 5.2 脚本要显式 stage

`DirectoriesToAlwaysStageAsUFS` 的路径相对**工程 Content 目录**：

| 路径 | 内容 |
|---|---|
| `Script` | 工程脚本 |
| `../Plugins/UnLuaNova/Plugins/UnLuaNova/Content/Script` | 插件运行时模块 |

漏掉第二条的表现很隐蔽：编辑器里一切正常，打包后 `require('UnLuaNova.Vector')` 失败。
（UnLuaNova 编辑器模块会自动补 `Script` 与它自己硬编码的旧路径，但补不了插件目录这条。）

## 6. 后端是编译期选择

`LuaBackend`（`Config/DefaultUnLuaNova.ini`）决定编哪个 VM。相关约束：

- 使用方模块的 `Build.cs` 必须**再列一次**后端模块（`Luau` 或 `Lua`）：
  UnLuaNova 的公共头文件直接调 `lua_*`，模板在使用方实例化，而 UBT 的依赖不递归传递链接项
- 两个后端不能同时列：同名 `lua.h` 会一起进 include 路径
- 切后端必须重编；ini 与二进制分叉会出现"目录叫 lua、跑的是 Luau"

脚本侧不要猜后端，用 `UnLuaNova.HasFeature(...)` / `GetBackend()` 按能力分支。

## 7. 变更检查清单

改到脚本相关的东西时，逐条确认：

- [ ] 核心模块（`UEAbilityKit` / `UEAbilityKitEditor`）没有新增任何脚本相关 include
- [ ] `UEAbilityKit.uplugin` 的 UnLuaNova 引用仍然是 `Optional`
- [ ] 新增的 Lua 脚本在打包路径覆盖范围内（工程脚本 / 插件模块）
- [ ] 切过后端的话，`Config` 的 ini 与编译产物是同一个后端
- [ ] 新增宿主适配代码放在适配层（`Source/LyraAdapt/`），没进插件

## 8. 已知限制

- 想让某个模块真正依赖 UnLuaNova（写 C++ 扩展），`Build.cs` 里必须**同时**列 `UnLuaNova`
  和后端模块（`Luau` 或 `Lua`）—— UBT 的依赖不递归传递链接项。列完用 `doctor` 自检
- 嵌套布局下 UnLuaNova 的扩展插件（`LuaSocket` / `LuaRapidjson` / `LuaProtobuf`）会被一起发现并编译，
  `setup --disable-extensions` 可一键关
- 脚本路径（示例模块的 Lua 绑定 + Luau 后端）的 PIE 实况仍待人工走查（[06 清单](../examples/06-live-trace-checklist.md)）
- UAK 不为脚本内容做校验：脚本写错了，框架层没有护栏，只能靠 UnLuaNova 的类型检查与运行时报错
