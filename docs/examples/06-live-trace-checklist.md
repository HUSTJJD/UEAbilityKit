# 实况联调清单：把 Trace 跑在 PIE 里

> M4 的数据结构与视图早已就位，缺的是「真在 PIE 里跑一遍」。
> 这份清单就是那一次联调的操作步骤与验收标准——照着走完，血缘树就不再是设计期推断，而是真实数据。

## 1. 前置条件

| 项 | 检查方式 |
|---|---|
| 示例工程能进 PIE | 打开 `LyraStarterGame.uproject`（宿主是 Lyra，`UAKDemo.uproject` 已删除），关卡里有 `BP_DemoCharacter` 并设为 `GameDefaultMap` |
| 角色被授予了技能 | `BP_DemoCharacter` 的 `Default Abilities` 里填了 `UAKDemoAbility` 的蓝图子类 |
| 技能指向了资产 | 该技能的 `AbilityAsset` 字段指向一个 `Ability Asset`（可用 `Plugins/UEAbilityKit/Content/Demo/Abilities/*.json` 导入） |
| Lua 后端可用（脚本路径通） | PIE 启动后日志里能看到 `[UAKDemo] Character BeginPlay`（示例角色由 `Content/Script/UAKDemoCharacter.lua` 绑定，见 [04](04-scripting-with-unlua.md)）；卡住时用 `lua.gc` / `lua.do` 确认 VM 活着 |

## 2. 开启记录

两种方式，任选其一：

```text
UAK.Trace.Enable          # 控制台命令，作用于所有 Game / PIE 世界
```

或者**打开技能资产编辑器**再进 PIE——编辑器会在 PIE 期间自动开启记录（`bPIERecordingForced`），
结束时自动恢复。

> 关闭时 `BeginTrace()` 直接返回 `nullptr`，**零开销**，不需要在发布版本里特意去掉。

## 3. 放一次技能

在 PIE 里触发技能（Lua 侧 `ActivateAbility`，见 [04-scripting-with-unlua.md](04-scripting-with-unlua.md)）。

## 4. 看结果

### 4.1 血缘视图（推荐）

开着资产编辑器进 PIE → 血缘 Tab **自动切到运行时实况数据源**，并且：

- 时间轴光标跟随最晚的溯源节点时间
- 树里出现的是**真实**节点：GE 类名、SetByCaller 数值、命中的目标名

### 4.2 控制台（不依赖编辑器）

```text
UAK.Trace.Dump            # 导出最近 1 条
UAK.Trace.Dump 5          # 最近 5 条（含进行中的）
UAK.Trace.Clear           # 清空已完成记录
UAK.Trace.Disable         # 关闭记录
```

输出形如：

```text
[Ability] AB_MeleeSwing @ 0.000s
  [Task] PlayMontage @ 0.000s
  [Task] ApplyGE #Damage @ 0.350s
    [GE] GE_MeleeDamage @ 0.350s  {SetByCaller Damage=30}
      [Target] BP_DemoCharacter_0 @ 0.350s
        [Attribute] Health 100 → 70 @ 0.350s
```

## 5. 验收标准

逐条打勾，全绿才算「实况联调通过」：

| # | 检查点 | 期望 |
|---|---|---|
| 1 | 根节点是技能资产名 | `[Ability]` 一行，Label = 资产名 |
| 2 | 每个进入的任务都有节点 | 任务数与「时间轴上被执行的任务」一致 |
| 3 | `TaskId` 生效 | 同类任务出现多次时，Label 用 TaskId 区分而不是都叫 `ApplyGE` |
| 4 | GE 下钻 | ApplyGE 任务下有 `[GE]` 节点，带 SetByCaller 明细 |
| 5 | 目标下钻 | CollisionQuery / AreaEffect 下有 `[Target]` 节点，带目标 Actor 名 |
| 6 | 属性变化 | 目标下出现 `[Attribute]`，记录前后数值 |
| 7 | 时间单调 | 各节点的 `Time` 随时间轴推进递增（循环时按本次循环记） |
| 8 | 中断不丢节点 | 被打断的技能仍能在 `Dump` 里看到（记录已在 `EndAbility` 时封口） |
| 9 | 关闭记录无开销 | `UAK.Trace.Disable` 后 `Dump` 不再新增记录 |
| 10 | 长时间运行不膨胀 | 连续放 50 次技能后 `FinishedTraces` 不超过 32 条 |

## 6. 常见问题

| 现象 | 原因 | 处理 |
|---|---|---|
| `Dump` 提示「没有溯源记录」 | 没开启记录，或技能根本没激活 | 先 `UAK.Trace.Enable`；确认 ASC 真的激活了技能 |
| 只有 `[Ability]` 一个节点 | 技能是瞬时的、没有任务，或记录在中途被 `Disable` | 检查资产里是否有任务 |
| 有 `[Task]` 但没有 `[GE]` | GE 为空 / 未设置 | 任务的 GE 字段为空时不会记账（这是预期行为） |
| 血缘视图没切实况 | 资产编辑器的 PIE 联动找的是**资产同名**的记录 | 确认 PIE 里放的就是当前编辑的这个资产 |

## 7. 相关文档

- 溯源设计：[../design/12-trace.md](../design/12-trace.md)
- 调试器（下一步）：`UAK.Debug.*` 命令，见 [../roadmap.md](../roadmap.md) 的 M6 一节
