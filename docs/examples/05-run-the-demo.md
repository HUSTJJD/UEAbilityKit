# 跑通示例工程

> 目标：从 clone 到"按一下鼠标左键放出技能"不超过 5 分钟，
> 且**全程不需要在编辑器里手工点资产**——所有内容由一条命令生成。

## 1. 三步走

```bash
# ① 拉仓库（含 UnLuaNova 子模块）
git clone --recursive https://github.com/HUSTJJD/UEAbilityKit.git

# ② 编译（或直接用 VS / Rider 打开 UAKDemo.uproject）
./Scripts/build.ps1                     # Windows
```

```powershell
# ③ 一键生成示例内容：技能资产 + 蓝图 + 关卡，并把关卡设为默认地图
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    .\UAKDemo.uproject -run=UAKDemoSetup -unattended -nosplash
```

然后用编辑器打开 `UAKDemo.uproject` 直接 **Play**。

## 2. 一条命令做了什么

| 产物 | 来源 | 说明 |
|---|---|---|
| `/Game/Demo/Abilities/*.uasset` | `Content/Demo/Abilities/*.json` | Json → 资产，与 `UAKAbilityJsonSerializer` 完全同一套字段 |
| `/Game/Demo/BP_SampleAbility` | 父类 `UAKDemoAbility` | 指向第一条时间轴 + C++ 的冷却 / 消耗 GE |
| `/Game/Demo/BP_DemoCharacter` | 父类 `AAKDemoCharacter` | Mannequin 网格 + 默认技能 + 初始属性 GE |
| `/Game/Demo/Maps/L_Demo` | 新建空白关卡 | 地面 + 平行光 + PlayerStart + 一个自动接管 Player0 的角色 |
| `GameDefaultMap` | 写进 `Config/DefaultEngine.ini` | 打开工程就落在示例关卡 |

GE 全部是 **C++ 类**（`Source/UAKDemo/UAKDemoEffects.h`），不是 `.uasset`：
仓库里不塞二进制，冷却 Tag 用原生 Tag 注册，不依赖任何 `.ini`。
（`UAKDemoSetupCommandlet` 的实现在 `Source/UAKDemo/UAKDemoSetupCommandlet.cpp`。）

## 3. 操作与预期现象

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

## 4. 输入链路（谁负责什么）

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

## 5. 实况调试

```text
UAK.Debug.Enable        # 角色头顶显示"在放什么技能、跑到第几秒、卡在哪个门控任务"
UAK.Debug.Summary       # 打到日志
UAK.Trace.Dump          # 导出血缘树：技能 → 任务 → GE → SetByCaller → 目标 → 属性
```

编辑器里还有调试面板：**窗口 → 开发者工具 → 调试 → UAK Debugger**，每 0.1 秒刷新一次活跃技能。
完整的实况联调清单见 [06-live-trace-checklist.md](06-live-trace-checklist.md)。

## 6. 不想用脚本

示例模块对 UnLuaNova 的依赖只在一处（`AAKDemoCharacter` 实现 `IUnLuaNovaInterface`）：

1. 从 `UAKDemo.Build.cs` 去掉 `"UnLuaNova"`
2. `AAKDemoCharacter` 去掉 `IUnLuaNovaInterface` 与 `GetModuleName_Implementation`
3. 把 `K2_OnAbilityInput` 改成 C++ 实现（直接调 `TryActivateAbilityByIndex`）
4. `.uproject` 里 `UnLuaNova` 设为 `false`

UAK 插件本身**不依赖任何脚本系统**，不受影响。
