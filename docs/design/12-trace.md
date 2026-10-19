# 溯源设计（已定稿，M4）

## 1. 要解决的问题

回答「这个 120 点伤害是哪来的」。
Able 只能告诉你"现在在跑哪个 Task"；AbilityKit 提出 Trace 溯源思想，我们把它落进 GAS 的世界。

## 2. 数据结构

```
FAKTraceNode
├─ ParentIndex   // INDEX_NONE 表示根；由此构成一棵树
├─ Kind          // Ability / Task / Effect / Cue / Target / Attribute
├─ Label
├─ Time
└─ Details[]     // 例如 "Health -120" / "SetByCaller Damage=120"
```

用 `ParentIndex` 而不是嵌套数组，是为了让整棵树能放进一个 `TArray`（可整体复制、序列化、跨线程传递）。

## 3. 记录链路

```
UAKTraceSubsystem（WorldSubsystem，bRecordingEnabled 开关）
   └─ BeginTrace(Label) → UAKTrace
         └─ UAKAbilityInstance::SetTrace(Trace)
               └─ StartDueTasks 时为每个任务建节点
   └─ EndTrace(Trace) → 移入 FinishedTraces（最多保留 32 条）
```

关闭记录时 `BeginTrace` 返回 `nullptr`，`Instance` 里所有写 Trace 的地方都是 nullptr 判断（零分配、零开销）。

## 4. 两个消费方

| 消费方 | 数据来源 | 用途 |
|---|---|---|
| 血缘视图（设计期） | `GetGASImpact()` 静态汇总 | 不跑游戏也能看到技能结构 |
| 血缘视图（运行时） | `UAKTrace` 实况 | PIE 里看到真实的 GE / 属性变化 |

## 5. PIE 联动

编辑器每 0.1s 扫 PIE 世界的 `UAKTraceSubsystem`，按资产名匹配最新一条记录：
- 血缘视图切到实况数据源
- 时间轴光标跟随到最晚节点时间

PIE 期间编辑器会自动打开 `bRecordingEnabled`（退出时不强制关，避免干扰项目自己的设置）。

## 6. 待补（M5）

- 效果任务写入 Effect / Target / Attribute 子节点（当前只到 Task 层）。
- Trace 导出为文件（配合 CI 与回放）。
- 与预测回滚的关系：被回滚的施放应丢弃对应 Trace——需要监听 `FPredictionKey` 的驳回。
