# 脚本方案：UnLuaNova 集成（git submodule，跟随 master）

> **[UnLuaNova](https://github.com/HUSTJJD/UnLuaNova)** —— 面向 Unreal Engine 的 Lua 脚本方案，[Tencent/UnLua](https://github.com/Tencent/UnLua) 的社区续作。
> UnLua 官方发布线冻结在 v2.3.6（2023-11）后无人维护，UnLuaNova 把散落的社区成果收敛成一条有人维护的发行线，
> 并补上三件事：**静态绑定**（构建期生成绑定表，绕开反射热路径）、**双 Lua 后端**（Lua 5.5 / Luau，默认 Luau）、
> **Lua 可观测性**（CPU Profiler 写入 Unreal Insights、`lua.dbg` 调试服务、`lua.sandbox` 沙盒、死循环看门狗）。
> 基线引擎 **UE 5.8.3**；从 UnLua 迁移只需把全局名 `UnLua` 批量换成 `UnLuaNova`。
>
> 📖 在线文档：<https://hustjjd.github.io/UnLuaNova/>　·　⭐ 仓库：<https://github.com/HUSTJJD/UnLuaNova>
> 许可：Polyform Small Business 1.0.0（与 UAK 同族；上游 UnLua 的 MIT 不变）。

**本文只讲 UAK 与它的集成方式**：目录怎么摆、插件怎么被发现、依赖怎么声明、后端怎么选、
脚本怎么随包、C++ 与 Lua 怎么分工、怎么整体关掉。
UnLuaNova 本身的用法请看[它的文档](https://hustjjd.github.io/UnLuaNova/)。

## 1. 集成现状一览

| 项 | 现状 |
|---|---|
| 引入方式 | git submodule，挂在项目 `Plugins/UnLuaNova/` |
| 版本 | 跟随 **master**（`v1.0.0` 之后的开发线；v1.0.0 = 2026-10-23 的首个正式版）。后端 pin 见插件 `Source/ThirdParty/VERSIONS.md`：**Lua 5.5.1** / **Luau 0.743+1** |
| 宿主工程 | 仓库根即工程根，示例随插件分发（见 [03-sample-project.md](03-sample-project.md)） |
| 插件发现 | UE 递归扫描项目 `Plugins/`，**不需要** `AdditionalPluginDirectories`；日志可见 `Mounting Project plugin UnLuaNova` |
| 谁依赖它 | **没有模块依赖它**：示例的 Lua 绑定走 ini 映射（数据驱动），`UAKDemo.Build.cs` 里一个 `UnLuaNova` 字都没有（见 §3） |
| UAK 插件本体 | 不依赖任何脚本系统；`.uplugin` 里的 UnLuaNova 引用标了 `"Optional": true` |
| Lua 后端 | **Luau**（默认），由 `Config/DefaultUnLuaNova.ini` 显式钉住 |
| 脚本落点 | 工程 `Content/Script/`（示例：`UAKDemoCharacter.lua`） |
| 打包 | `DirectoriesToAlwaysStageAsUFS`（工程脚本 + 插件运行时模块），见 §6；上游已改为按插件实例自动补 |
| 静态检查 / 智能提示 | 产物在 `Plugins/UnLuaNova/Plugins/UnLuaNova/IntelliSense/{EmmyLua,Luau}`；首次启动编辑器自动生成（见 §7） |
| 一键自检 | `./Tools/unluanova.ps1 setup` / `doctor`（见 §2.1） |

一句话：**UAK 是纯 C++ 的产品插件，脚本只服务示例与项目扩展；想接就接，不想接删掉目录也能编译。**

## 2.1 setup / doctor：把"知道才会写对"变成机器可查

集成里最贵的从来不是写代码，是"忘了某一处、然后在打包后炸"。这两条命令就是为它准备的：

```powershell
./Tools/unluanova.ps1 setup      # 子模块 / 后端 ini / stage 路径 / 扩展插件，一次写对
./Tools/unluanova.ps1 doctor     # 只读体检，有 error 就退出码 1（可直接当 CI 门禁）
```

`doctor` 查的正是历史上真实踩过的坑：

| 检查 | 抓的是什么 |
|---|---|
| `submodule` / `third-party` | 子模块没拉全（嵌套的 Lua / Luau 最容易忘） |
| `backend-ini` / `backend-binary` / `build-cs` | **ini ↔ 二进制 ↔ Build.cs 三处后端是否分叉**（分叉的表现是"目录叫 lua、跑的是 Luau"） |
| `stage` | 打包 stage 路径有没有漏插件运行时模块那条 |
| `binding` | 数据驱动绑定是否**成对**：只配 `ClassModuleBindings` 而没开 `bDataDrivenBinding` → error（不报错、不生效，最容易白忙一场的一处） |
| `extensions` | `LuaSocket` / `LuaRapidjson` / `LuaProtobuf` 的启用状态（上游 `EnabledByDefault=false`，没写就是没开） |
| `intellisense` | 有没有产物（没产物 = 没补全 = "这方案不好用"的错觉） |

`setup` 逐条修，且是**幂等**的：已经对了就只报告。

```powershell
./Tools/unluanova.ps1 setup --backend Lua   # 切后端（之后必须重编）
./Tools/unluanova.ps1 setup --disable-extensions --dry-run
```

## 2. 为什么是 submodule，目录怎么摆

UnLuaNova 有自己的仓库与版本节奏，不应该被复制进 UAK：

- 保持**单一来源**：升级 UnLuaNova 只需移动 submodule 指针
- UAK 的仓库体积不被第三方源码撑大
- 示例工程里 Lua 与 C++ 的边界一眼可见

```bash
git clone --recursive https://github.com/HUSTJJD/UEAbilityKit.git
# 已经克隆过的：
git submodule update --init --recursive
```

!!! warning "别忘了 `--recursive`"
    UnLuaNova 自身还嵌套了 Lua / Luau（以及 `luau-lsp`、`lua-language-server` 等工具）子模块。忘了就补上面那条。

UnLuaNova 仓库本身是一个 UE 工程，**真正的插件在它内层的 `Plugins/` 里**：

```
Plugins/UnLuaNova/                                ← submodule 根（= UnLuaNova 示例工程）
├─ Content/Script/                                ← 它自己示例工程的脚本（本工程用不到）
└─ Plugins/
   ├─ UnLuaNova/UnLuaNova.uplugin                 ← 脚本方案本体（我们要的）
   │  ├─ Content/Script/UnLuaNova/                ← 随插件发布的 Lua 运行时模块
   │  │                                             （StringEx / Vector / Buffer / Input /
   │  │                                              EnhancedInput / HotReload）
   │  ├─ Source/ThirdParty/Luau/                  ← Luau VM（子模块）
   │  └─ Binaries/Win64/UnrealEditor-Luau.dll     ← 后端 VM 也在这里
   ├─ UnLuaNovaExtensions/{LuaSocket,LuaRapidjson,LuaProtobuf}/
   └─ UnLuaNovaTestSuite/                         ← `EnabledByDefault=false`，不参与本工程构建
```

关于发现方式：UE 会**递归**扫描项目的 `Plugins/` 目录，所以这种"工程套插件"的嵌套布局不用额外配置，
`.uproject` 里既没有 `AdditionalPluginDirectories`、也没有 UnLuaNova 的开关条目 ——
编辑器启动时日志里有 `Mounting Project plugin UnLuaNova` 就是证据。

关于内层的扩展插件：**它们现在默认是关的**。
`LuaSocket` / `LuaRapidjson` / `LuaProtobuf`（都在 `Plugins/UnLuaNovaExtensions/` 下）
的 `.uplugin` 里 `EnabledByDefault` 是 `false` —— 宿主 `.uproject` 里没写就是没开，
要用就在 `.uproject` 里显式写 `"Enabled": true`（三者互不依赖，可以只开一个）。
脚本侧 `require("socket.core")` / `require("rapidjson")` / `require("pb")` 才能用。
`UnLuaNovaTestSuite` 同样是关的，不用管。

!!! note "被这条文档骗过的人不少"
    早先的 UnLuaNova 把三个扩展插件的 `EnabledByDefault` 设成 `true`，于是"嵌套布局下会被
    一起编译"。现在反过来了：`setup --disable-extensions` 存在的意义也从"救火"变成"钉死"
    —— 防止别处（模板工程、同事的分支）把它们带起来。

## 3. 依赖声明

**现在没有任何模块依赖脚本系统** —— 连示例模块也没有：

```csharp
// Plugins/UEAbilityKit/Source/UAKDemo/UAKDemo.Build.cs
PublicDependencyModuleNames.AddRange(new[] {
    "Core", "CoreUObject", "Engine",
    "GameplayAbilities", "GameplayTags",
    "UEAbilityKit"
    // 没有 "UnLuaNova"：绑定是配置出来的，不是编译出来的（见 §4）
});
```

之所以能做到，是因为 Lua 绑定从"实现 `IUnLuaNovaInterface`"改成了**数据驱动**（§4），
`AAKDemoCharacter` 里一行脚本相关代码都没有。收益有两个：

1. 删掉 `Plugins/UnLuaNova` 整个目录，工程照样编译、照样跑测试（§9 从"改三处代码"缩成"删一个目录"）
2. 顺带躲开"使用方 Build.cs 必须再列一次 Lua 后端模块"那个坑（UBT 的依赖不递归传递链接项，
   少列就 LNK2019）—— 没有脚本依赖，也就没有后端

!!! note "要写 C++ 扩展时才需要依赖"
    只有当你的模块要 include UnLuaNova 的头文件（比如自己实现 ModuleLocator、调 `FLuaEnv`）时，
    才需要把 `"UnLuaNova"` **和后端模块**一起列进 `Build.cs`，两者缺一不可：

    ```csharp
    PublicDependencyModuleNames.Add("UnLuaNova");
    PrivateDependencyModuleNames.Add("Luau");   // 或 "Lua"，须与 ini 的 LuaBackend 一致
    ```

    列完跑一次 `./Tools/unluanova.ps1 doctor`，它会替你核对三处后端是否一致。

`UEAbilityKit.uplugin` 里那条 UnLuaNova 引用是**可选**的：

```json
{ "Name": "UnLuaNova", "Enabled": true, "Optional": true }
```

`Optional` 的意义：把 `Plugins/UEAbilityKit` 单独拷进一个没有 UnLuaNova 的项目时，
引擎只会记一条 Verbose 日志然后跳过，而不是报"引用了不存在的插件"。
**没有它，UAK 就无法独立分发** —— 这与"UAK 插件零脚本依赖"的立场是配套的。

!!! note "为什么模块级依赖不能省"
    Lua 后端（`Luau` 或 `Lua`）必须在使用方模块里**再列一次**：
    UnLuaNova 的公共头文件直接调 `lua_*`（模板在本模块实例化），
    而 UBT 的模块依赖不递归传递链接项 —— 少了它，符号会解析到错误的后端或直接 LNK2019。
    详见 `UAKDemo.Build.cs` 里的注释（它按 ini 读后端，避免与插件自身的编译期选择分叉）。

## 4. 分工：核心 C++，扩展用 Lua

| 层 | 用什么 | 原因 |
|---|---|---|
| 执行器、任务基类、编辑器、序列化 | **C++** | 需要强类型、反射、校验、可静态分析 |
| 内置任务（GE / Montage / Cue / 查询 / 投射物…） | **C++** | 通用能力，所有项目共享 |
| 输入绑定、UI 联动、一次性玩法逻辑 | **Lua** | 迭代快、项目差异大 |
| 自定义任务 | **Lua（通过 C++ 子类转发）** | 项目特有行为，不值得进框架 |
| 自定义引导条件（如"按住按键"） | **Lua 或 C++ 子类** | UAK 刻意不内置输入系统 |

UAK 留出的**脚本扩展点**目前有两个：

1. **数据驱动绑定**（默认路径，示例就用它）：类到模块的映射写在配置里，UE 侧一行代码都不用改。
   `Config/DefaultUnLuaNova.ini`：

   ```ini
   [/Script/UnLuaNova.UnLuaNovaSettings]
   ; 数据驱动绑定默认**关**：不配置就绝不给"没实现 IUnLuaNovaInterface"的对象绑脚本。
   ; 这是一条安全边界（开着时凡是匹配上映射表或绑定组件的对象都会被绑），不是性能开关。
   bDataDrivenBinding=true
   +ClassModuleBindings=(Class="/Script/UAKDemo.AKDemoCharacter",ModuleName="UAKDemoCharacter")
   ; 原生类也可以只写短名（同样沿父类链匹配，蓝图子类一并覆盖）：
   ; +ClassModuleBindings=(ClassName="AKDemoCharacter",ModuleName="UAKDemoCharacter")
   ```

   !!! warning "只写映射表不会生效"
      少了 `bDataDrivenBinding=true`，映射表配了也不绑 —— 定位器给出模块名之后
      **要不要真的绑**由这个开关决定（默认关，与旧版本行为逐字一致）。

   示例里这一条把 `AKDemoCharacter`（以及它的蓝图子类 `BP_DemoCharacter`，匹配会沿继承链向上找）
   指到 `Content/Script/UAKDemoCharacter.lua`；该脚本实现的是 C++ 上的
   `BlueprintImplementableEvent`（`K2_OnAbilityInput` / `K2_OnAbilityStarted` / `K2_OnAbilityEnded`），
   也就是项目侧逻辑。

   三种绑法任选，优先级从高到低（都在 `ULuaModuleLocator_ByBinding` 里，是默认定位器）：

   | 方式 | 怎么用 | 适合 |
   |---|---|---|
   | **ini 映射**（示例用这个） | 上面那条配置 | 不改 UE 侧代码，改配置就生效；**基类能覆盖蓝图子类** |
   | **绑定组件** | 蓝图 Actor 上加 `UnLuaNova Binding` 组件，填模块名 | 纯蓝图团队，按资产逐个配 |
   | **实现 `IUnLuaNovaInterface`** | C++ 或蓝图里覆写 `GetModuleName()` | 老项目迁移、模块名要按实例算的场景 |

   前两种都不需要碰 C++，这也是"示例模块对脚本零依赖"的前提。

   !!! warning "Lua 里的函数名必须与 UFUNCTION **同名**"
       覆写是按 UFunction 的名字去 Lua 表里找的（`UUnLuaNovaManager::Bind` 里的
       `UEFunctions.Find(LuaFuncName)`），**`K2_` 前缀不会被去掉**。所以示例里写的是

       ```lua
       function M:K2_OnAbilityInput(SlotIndex, bPressed)   -- 不是 OnAbilityInput
       ```

       写错不会报错，只是永远不触发 —— 这比报错更难查。`ReceiveBeginPlay` / `ReceiveEndPlay`
       这类引擎自带事件名本身就是 UFUNCTION 名，照抄即可。

   绑定完成后的钩子也有现成的：`UUnLuaNovaManager::Bind` 会调用模块里的 `Initialize`
   （比 `ReceiveBeginPlay` 更早，且不需要 C++ 侧再加回调接口）。
2. **引导条件的"状态喂给 C++"**：脚本调 `SetChannelingInputHeld(Slot, bPressed)` 把"按键还按着吗"
   写进角色；C++ 侧的条件对象 `UAKDemoChanneling_InputHeld`（派生 `UAKChannelingCondition`）
   每帧读这个状态来判定引导是否成立。

注意第 2 点的方向：**不是"脚本回答条件"，而是"脚本给 C++ 条件对象喂状态"**。
判定逻辑仍在 C++（可被校验、可进 Json、缺脚本时默认成立不会把示例卡死）；
宿主侧同一个扩展点也可以换成纯 C++ 的条件对象（读 Enhanced Input），
Lua 一行都不用 —— 这正是它被设计成"条件对象"而不是"Lua 钩子"的原因。

```lua
-- Content/Script/UAKDemoCharacter.lua（节选）
function M:OnAbilityInput(SlotIndex, bPressed)
    HeldSlots[SlotIndex] = bPressed

    -- 把"还按着吗"交给 C++：UAKDemoChanneling_InputHeld 每帧读它
    self:SetChannelingInputHeld(SlotIndex, bPressed)

    if not bPressed then return end
    self:TryActivateAbilityByIndex(SlotIndex)   -- 走 GAS 的冷却 / 消耗 / 预测
end
```

这样"松手即打断蓄力"这个**项目特有**的输入语义由脚本提供，判定与校验仍留在 C++；
UAK 只给出一个类型安全的扩展点 —— 框架不会被任何一种输入系统绑死。

## 5. Lua 后端：默认是 Luau

后端是**编译期**选择（两个后端的头文件不是同一份），切一次要重编。

| | Lua 5.5 | Luau（默认） |
|---|---|---|
| 定位 | plain Lua，第三方库生态最好 | 原生 `vector` / `buffer`、沙盒、原生断点与单步 |
| UAK 现状 | 可用，未作为默认 | 默认；`Config/DefaultUnLuaNova.ini` 里钉住 |

切换方式（改完必须重新编译，不能只重启编辑器）：

```ini
; Config/DefaultUnLuaNova.ini（示例工程自带）
[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]
LuaBackend=Luau      ; 或 Lua
```

> `UAKDemo.Build.cs` 读的是**同一个键**，
> 所以"编辑器设置"和"编译出来的后端"不会分叉。键不存在时 UnLuaNova 自己默认 Luau —— 我们不依赖这个隐式默认。

脚本里不要猜后端，用能力查询：

```lua
if UnLuaNova.HasFeature('sandbox') then ... end
local backend = UnLuaNova.GetBackend()   -- "Luau" / "Lua"
```

## 6. 打包：脚本要跟着进包

`DirectoriesToAlwaysStageAsUFS` 的路径**相对工程 Content 目录**（引擎 AutomationTool 就是这么解析的），
所以回到工程根要写 `..`。`Config/DefaultGame.ini` 里有两条：

```ini
[/Script/UnrealEd.ProjectPackagingSettings]
+DirectoriesToAlwaysStageAsUFS=(Path="Script")
+DirectoriesToAlwaysStageAsUFS=(Path="../Plugins/UnLuaNova/Plugins/UnLuaNova/Content/Script")
```

| 路径 | 装的是什么 |
|---|---|
| `Script` | 工程自己的脚本（`Content/Script/**`） |
| `../Plugins/UnLuaNova/Plugins/UnLuaNova/Content/Script` | 插件随包发布的运行时模块（`UnLuaNova.StringEx` 等） |

!!! note "这条以前最容易写错，现在上游会自动补"
    曾经这里必须手写成 `../Plugins/UnLuaNova/Plugins/UnLuaNova/Content/Script`
    （submodule 是"工程套插件"，真正的插件在内层）。写成 `../Plugins/UnLuaNova/Content/Script`
    会指向 submodule 里**示例工程**的脚本（Tutorials / Benchmark），编辑器里一切正常、
    打包后 `require('UnLuaNova.Vector')` 才失败。

    现在 UnLuaNova 编辑器模块按**插件实例**枚举（凡是名字以 `UnLuaNova` 开头、
    且带 `Content/Script` 的插件都补上），工程套插件 / 插件套插件都不会漏。
    仍建议跑一次 `./Tools/unluanova.ps1 doctor` 确认，别靠肉眼。

这两行直接写在示例工程的 `Config/DefaultGame.ini` 里，改完重启编辑器生效。

## 7. 智能提示与静态检查

**产物目录不存在时，编辑器启动会自动导出一次**（`bAutoGenerateIntelliSenseOnStartup`，默认开）。
这一步省掉的是"装完插件还得知道要去工具栏点一下" —— 没产物就没有补全，而"没有补全"
往往被误判成"这个方案不好用"。已经生成过就交给工具栏 / 目录监听做增量，不会拖慢每次启动。

手动触发仍然是编辑器工具栏导出（或 `UnrealEditor-Cmd.exe <uproject> -run=UnLuaNovaIntelliSense -BP=1`），
产物落在 **`Plugins/UnLuaNova/IntelliSense/`**：

```
Plugins/UnLuaNova/IntelliSense/
├─ EmmyLua/     Lua 方言：UE.lua / UnLuaNova.lua / <模块>/<类型>.lua → IDE 补全 + LuaLS
└─ Luau/        Luau 方言：UnLuaNova.d.lua → luau-lsp
```

写 UAK 的 Lua 脚本时，把这个目录指给 IDE（`workspace.library`），`UE.UAKDemoCharacter`、
`UE.UAbilitySystemComponent` 这些就有补全了。注意两种方言分目录是因为
**两个分析器解析不了对方的语言**，别把 `.d.lua` 喂给 LuaLS。

## 8. 调试：Lua 与 UAK 的两套可观测性要一起用

| 想看什么 | 用什么 |
|---|---|
| 技能为什么这么打（血缘树） | `UAK.Trace.Enable / Dump`（见 [06-live-trace-checklist.md](06-live-trace-checklist.md)） |
| 运行时实例快照 | `UAK.Debug.Enable / Summary`（见 [05-run-the-demo.md](05-run-the-demo.md)） |
| Lua 侧 CPU 热点 | `lua.profiler on`（写入 Unreal Insights） |
| Lua 内存 | `lua.memstats [on|off|reset]` |
| **IDE 里断点 / 单步 / 看变量** | **`lua.dap start [port]`**（DAP 服务，见 §8.1） |
| 命令行断点 / 单步 | `lua.dbg`（headless 调试服务，DAP 的下层） |
| 强制 GC / 执行一段代码 | `lua.gc`、`lua.do <code>`、`lua.dofile <module>` |
| 沙盒（限制脚本改写引擎类型） | `lua.sandbox`（**启动期 CVar**，不是运行时命令） |

定位"脚本把技能玩坏了"时，典型顺序是：`UAK.Trace.Dump` 找到可疑任务 →
`lua.profiler` 看是不是脚本热点 → `lua.dap start` 后在 IDE 里下断点。

### 8.1 DAP：把 headless 调试接到 IDE 上

`FLuaDebugService` 刻意不带 socket、不定义线协议（"that belongs in the DAP layer"），
于是"能调试"和"能在 VSCode 里调试"之间一直缺一层。`lua.dap` 就是那一层：

```
VSCode ──DAP/JSON──► lua.dap（编辑器进程内，自带线程）──► FLuaDebugService
```

```text
lua.dap start [port]     # 默认 5699；起服务，等 IDE 来 attach
lua.dap status
lua.dap stop
```

VSCode 侧用 attach 方式连 `127.0.0.1:5699` 即可（`request: "attach"`）。已实现的请求：
`initialize` / `attach` / `setBreakpoints` / `threads` / `stackTrace` / `scopes` /
`variables` / `continue` / `next` / `stepIn` / `stepOut` / `disconnect`。

| 行为 | 说明 |
|---|---|
| 命中断点 | 游戏线程**阻塞自旋**等 `Continue()`（DAP 的 blocking 模式），IDE 才有机会看栈与变量 |
| 暂停中改断点 | 要等 `Continue()` 之后才生效（游戏线程阻塞着，排不进去） |
| 不支持 | `pause`（服务没有强制中断接口）、`evaluate`（求值要跑在 Lua 所在线程） |

## 9. 不使用脚本时的取舍

**删掉目录就行，不用改代码**（示例的绑定是配置出来的，不是编译出来的）：

1. 删除 `Plugins/UnLuaNova/`（submodule）与工程 `Content/Script/`
2. 宿主 `.uproject` 里把 `UnLuaNova`（及三个扩展插件）设为 `false`；
   `Config/DefaultGame.ini` 里的 stage 路径、`Config/DefaultUnLuaNova.ini` 也可以去掉

`UAKDemo` 的 `Build.cs` 与 `AAKDemoCharacter` **都不用动** —— 它们本来就没有脚本依赖。
插件侧的 `Optional` 依赖也不用动 —— 缺了脚本插件它本来也不会拦你。

## 10. 热更新：这是选 Lua 而不是别的脚本方案的核心理由

编辑器里改完 `.lua` 直接生效（工具栏 HotReload，或 `EHotReloadMode=Auto` 的目录监听）；
运营期则是另一回事：**脚本下发**是 Lua 相对静态脚本方案（如 AngelScript）唯一的独占优势，
值得按一等能力来对待，而不是当成一条 feature。

| 场景 | 机制 |
|---|---|
| 编辑器内迭代 | HotReload：`IUnLuaNovaModule::HotReload()` → `Lua.HotReload()` → 重 require 模块 |
| PIE 内 | 非结构性改动可不退出播放会话直接重载 |
| 运营期下发 | 脚本不在 cooked 资产里，走 `DirectoriesToAlwaysStageAsUFS` 之外的内容分发通道即可 |

配套的安全边界：`lua.sandbox`（限制脚本改写引擎类型）、死循环看门狗（`DeadLoopCheck`）、
`lua.profiler` 兜住"下发了一段慢脚本"。**下发链路本身要项目自己建**，UAK 不提供也不约束。

## 11. 内置库、特性开关与静态绑定

这三件事上游都有了，接入时只要知道它们**在哪儿开关、改了要不要重编**。

### 11.1 内置库（StdLib）

随插件发布在 `Content/Script/UnLuaNova/`（已被 §6 那条 stage 路径覆盖），`require` 直接用：

```lua
local Timer = require('UnLuaNova.Timer')   -- C++ 实现：走世界时间，暂停 / 时间膨胀 / PIE 生命周期都对
local Event = require('UnLuaNova.Event')   -- 纯 Lua：事件总线（广播中增删也安全）
local OOP   = require('UnLuaNova.OOP')     -- 纯 Lua：脚本面向对象
```

上游的取舍标准值得抄：**只放"机制"，不放"策略"**。一旦发布 `UnLuaNova.X` 就承诺了 API 稳定，
所以"每个项目都要重写一遍且都会写错"的（计时器、事件总线、热重载）进库，
"一半人会重写"的（UI 栈、HTTP、Model 层）不进。同理，别为了性能把东西下沉 C++ ——
回调还在 Lua 就省不掉那次跨界，`Timer` 下沉是因为正确性只能由引擎给。

### 11.2 特性开关：全插件只有一处定义

`Config/DefaultUnLuaNova.ini` 的 `[/Script/UnLuaNova.UnLuaNovaSettings]`，规则固定三条：

1. 键名一律 `b<CamelCase>`，查询名就是去掉 `b` 的小写形式（`bTimer` → `HasFeature('timer')`）
2. 一个入口：脚本侧 `UnLuaNova.HasFeature('<名>')`，C++ 侧 `UnLuaNovaLib` 那张表 —— 没有第二处
3. 大小写不敏感（`oop` 与 `OOP` 同义）

```ini
[/Script/UnLuaNova.UnLuaNovaSettings]
bDataDrivenBinding=true   ; 数据驱动绑定（默认关，见 §4）
bTimer=true               ; 内置库开关是**编译期**的：改完必须重编
bDebugAdapter=true        ; lua.dap
DebugAdapterPort=5699     ; lua.dap 默认端口
```

!!! warning "内置库开关要重编"
    `bTimer` / `bEvent` / `bOOP` 同时由 `UnLuaNova.Build.cs` 读**同一份 ini** 生成
    `UNLUANOVA_HAS_STDLIB_*` 宏 —— 关掉的代码不进二进制。这是"编译期 vs 运行时"的唯一例外，
    其余特性（含 `bDataDrivenBinding`）都是运行时的。

### 11.3 静态绑定：编辑器会自动生成，产物是 C++

`bStaticBinding` / `bAutoGenerateStaticBinding` 默认都是开：编辑器启动时比对产物里的类型集合指纹，
缺失或类型有增删就**自动**重新生成，不用手动跑 Commandlet。产物是 C++，**生成之后要重编译一次**才生效。

代价要知道：默认范围（`ExportedReflected`）下重编很轻；把 `StaticBindingSelection` 切到 `All`
之后类型增删会频繁触发大重编译 —— 那种场景更适合关掉自动、交给 CI 生成。

### 11.4 C++ 调 Lua：上游给了入口，但判定我们仍留在 C++

现在可以从 C++ 直接调某个对象上的 Lua 成员函数：

```cpp
#include "UnLuaNovaOverrider.h"
bool bEnabled = UnLuaNova::CallLuaFunction<bool>(MyActor, TEXT("IsTestEnable"), 1, 2);
```

这让"引导条件由脚本回答"在技术上可行了。**UAK 仍然不这么用**（§4 第 2 点）：条件对象是可校验、
能进 Json、缺脚本时默认成立的类型安全扩展点；一旦把判定搬进 Lua，这三条一起失效。
要不要用它由项目自己决定 —— 入口在那儿，不是我们故意不给你。

## 12. 已知限制

| 限制 | 说明 |
|---|---|
| 扩展插件默认**关** | `LuaSocket` / `LuaRapidjson` / `LuaProtobuf` 的 `EnabledByDefault=false`，要用就在 `.uproject` 里显式开（见 §2） |
| 后端切换要重编 | 见 §5；ini 与二进制分叉会导致"跑的是另一个后端"（`doctor` 会抓） |
| 改内置库开关要重编 | `bTimer` / `bEvent` / `bOOP` 是编译期开关（见 §11.2） |
| 打包路径 | 见 §6；插件运行时模块那条必须写对 |
| DAP 只覆盖 attach | 不支持 launch（调试目标是编辑器 / PIE 进程，不是独立进程） |
| 覆写函数名 | 必须与 UFUNCTION 同名（`K2_` 前缀不去），写错静默不触发（见 §4） |
| 数据驱动绑定的缓存 | 打包构建里"这个类没绑"的结论会缓存；编辑器里不缓存，改配置/组件即时生效 |
| 实例级绑定未支持 | `UnLuaNovaManager::Classes` 仍按类缓存，同一个类的不同实例不能绑不同模块（上游 BIND-01） |
| 网络是实验的 | 声明式 RPC / 复制属性已有实现但仍有阻塞项；**普通覆写与声明 RPC 是不同路径**，别从 `IsOverridable` 一个分支推导全部网络能力 |
| 脚本路径待人工走查 | 示例脚本与 Luau 后端的 PIE 实况按 [06 清单](06-live-trace-checklist.md) 逐条确认 |

## 13. 延伸阅读

| 内容 | 入口 |
|---|---|
| 集成契约与所有权归属 | [design/20-scripting-integration.md](../design/20-scripting-integration.md) |
| UnLuaNova 在线文档 | <https://hustjjd.github.io/UnLuaNova/> |
| UnLuaNova 仓库 | <https://github.com/HUSTJJD/UnLuaNova> |
| 编程指南 / 设置 / 控制台命令 / 智能提示 | 见其仓库 `Docs/CN/`（UnLuaNova 侧） |
| Lua 驱动对象（ini 映射 / 绑定组件 / 接口三条路） | `Docs/CN/LuaDrivenObjects.md`（UnLuaNova 侧） |
| 内置库与特性开关 | `Docs/CN/StdLib.md` |
| 静态导出与静态绑定 | `Docs/CN/StaticExportBinding.md` |
| 网络（实验） | `Docs/CN/LuaNetworking.md` |
| 后端 pin 版本 | `Plugins/UnLuaNova/Source/ThirdParty/VERSIONS.md` |
| 从 UnLua 迁移 | 全局名 `UnLua` → `UnLuaNova`，UCLASS 改名需 resave 资产并迁移 ini section |
| 工程是怎么组织的 | [03-sample-project.md](03-sample-project.md) |
