# 跑通示例

> 目标：从导入到"按一下鼠标左键放出技能"全程不需要在编辑器里手工点资产——
> 所有内容由一条命令生成，**且生成在插件自己的 Content 里**。

## 1. 现在的形态

`UAKDemo` **不是独立工程**，它是插件的一个模块：

```
Plugins/UEAbilityKit/
   ├─ Plugins/UEAbilityKit/Source/UAKDemo/          ← 示例模块（随插件分发）
   └─ Plugins/UEAbilityKit/Content/Demo/            ← 示例资产（Json + 动画，插件自己管）
```

宿主工程是改造过后的 Lyra（见 [lyra-integration.md](lyra-integration.md)）。

## 2. 三步走

```powershell
# ① 导入 Lyra（拷源码 + 外围文件 + 资产，然后打补丁）
cd Tools
./lyra.ps1 import --lyra-path <你的 Lyra 目录>

# ② 编译
cd ..
./Tools/build.ps1
```

```powershell
# ③ 一键生成示例：技能资产 + 蓝图 + 关卡（全部落在 /UEAbilityKit/ 下）
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    .\LyraStarterGame.uproject -run=UAKDemoSetup -unattended -nosplash
```

想让示例关卡成为默认地图，显式加 `-SetDefaultMap`——
**默认不改宿主的 `GameDefaultMap`**，否则会把 Lyra 自己的流程顶掉。

## 3. 一条命令做了什么

| 产物 | 来源 | 说明 |
|---|---|---|
| `/UEAbilityKit/Demo/Abilities/*.uasset` | `Plugins/UEAbilityKit/Content/Demo/Abilities/*.json` | Json → 资产，与 `UAKAbilityJsonSerializer` 完全同一套字段 |
| `/UEAbilityKit/Demo/BP_SampleAbility` | 父类 `UAKDemoAbility` | 指向第一条时间轴 + C++ 的冷却 / 消耗 GE |
| `/UEAbilityKit/Demo/BP_DemoCharacter` | 父类 `AAKDemoCharacter` | Mannequin 网格 + 默认技能 + 初始属性 GE |
| `/UEAbilityKit/Demo/Maps/L_Demo` | 新建空白关卡 | 地面 + 平行光 + PlayerStart + 一个自动接管 Player0 的角色 |

GE 全部是 **C++ 类**（`Plugins/UEAbilityKit/Source/UAKDemo/UAKDemoEffects.h`），不是 `.uasset`：
仓库里不塞二进制，冷却 Tag 用原生 Tag 注册，不依赖任何 `.ini`。
（commandlet 实现在 `Plugins/UEAbilityKit/Source/UAKDemo/UAKDemoSetupCommandlet.cpp`。）

**为什么资产落在 `/UEAbilityKit/` 而不是 `/Game/`**：
插件自己管自己的示例，被拷到任何工程都能一键生成，不会往宿主工程的 Content 里写东西。

## 4. 操作与预期现象

| 操作 | 期望 |
|---|---|
| 鼠标左键 / 数字键 1 | 近战：Montage + 伤害 GE + Cue |
| 鼠标右键 / 数字键 2（**按住**） | 引导火球：松手或移动过快即打断 |
| 连续按左键 | 第二次被冷却挡住（5 秒，日志里能看到 `Cooldown.Demo.Sample`） |
| 蓝量见底 | 消耗 GE 拒绝激活（Mana -20） |

Lua 侧会打印：

```text
[UAKDemo] Character BeginPlay
[UAKDemo] 输入：近战挥砍
[UAKDemo] Started: AB_MeleeSwing (第 1 次)
[UAKDemo] Ended: AB_MeleeSwing
```

## 5. 输入链路（谁负责什么）

```
DefaultInput.ini（键位）
   ↓ BindAction
AAKDemoCharacter::SetupPlayerInputComponent   ← C++：只转发"按下/松开"
   ↓ K2_OnAbilityInput(Slot, bPressed)
Content/Script/UAKDemoCharacter.lua           ← Lua：决定做什么
   ↓ TryActivateAbilityByIndex(Slot)
GAS（冷却 / 消耗 / 预测）→ UAKGameplayAbility → UAKAbilityInstance（时间轴）
```

分工的边界很清楚：

- **C++**：绑输入（API 不是 UFUNCTION，脚本调不到）、ASC、属性、技能授予
- **Lua**：按下之后干什么、UI 读条、`SetChannelingInputHeld` 反馈"还按着吗"
- **UAK**：只管把时间轴跑起来

引导条件的"按住"语义由 `UAKDemoChanneling_InputHeld` 提供（派生 UAK 的
`UAKChannelingCondition`）——UAK 刻意不内置输入系统，只留类型安全的扩展点。

## 6. 实况调试

```text
UAK.Debug.Enable        # 角色头顶显示"在放什么技能、跑到第几秒、卡在哪个门控任务"
UAK.Debug.Summary       # 打到日志
UAK.Trace.Dump          # 导出血缘树：技能 → 任务 → GE → SetByCaller → 目标 → 属性
```

编辑器里还有调试面板：**窗口 → 开发者工具 → 调试 → UAK Debugger**，每 0.1 秒刷新一次活跃技能。
完整的实况联调清单见 [06-live-trace-checklist.md](06-live-trace-checklist.md)。

## 7. 不想用脚本

**删掉目录就行，不用改代码**。示例的 Lua 绑定是配置出来的（ini 里的类→模块映射），
不是编译出来的，所以 `UAKDemo.Build.cs` 与 `AAKDemoCharacter` 里一个脚本相关的字都没有。

1. 删除 `Plugins/UnLuaNova/`（submodule）与工程 `Content/Script/`
2. 宿主 `.uproject` 里 `UnLuaNova` 设为 `false`（嵌套的 `LuaSocket` / `LuaRapidjson` /
   `LuaProtobuf` 同理，或跑 `Tools/unluanova.ps1 setup --disable-extensions`）
3. `Config/DefaultGame.ini` 里的 `DirectoriesToAlwaysStageAsUFS`、
   `Config/DefaultUnLuaNova.ini` 可以去掉
4. 输入链路会退回 C++：`K2_OnAbilityInput` 是 `BlueprintImplementableEvent`，没有脚本实现
   时它就是个空事件 —— 想保留"按键激活技能"，在蓝图里实现它并直接调 `TryActivateAbilityByIndex`。
   引导条件的"按住"语义本来就是 C++ 读角色状态（`UAKDemoChanneling_InputHeld`），
   去掉脚本后默认成立，不会把示例卡死

> 反过来，想启用脚本：拉 submodule → `./Tools/unluanova.ps1 setup` → `doctor` 复检。
> 中途任何一步"忘了"，`doctor` 会直接告诉你缺哪条。

插件侧不用动：`UEAbilityKit.uplugin` 里的 UnLuaNova 引用本来就是 `Optional`，
**UAK 插件核心不依赖任何脚本系统**，不受影响。
