# 复合编排设计（已定稿，M5）

## 1. 思想来源与落地方式

AbilityKit 用 **Phase 图**表达组合（Sequence / Parallel / Repeat / Delay / WaitUntil / Conditional）。
UAK 不引入图结构，而是把组合语义放进**轨道**：

> 任务本来就是"时间轴上的片段"，"这一串片段怎么排"天然属于轨道的属性。

好处：编辑器仍是那条看得懂的时间轴，不需要第二套图编辑器；执行器仍是扁平调度表，不需要递归。

## 2. 三个模式

| 模式 | 任务时间如何确定 | 典型用法 |
|---|---|---|
| `Timeline`（默认） | 用任务自己的 `StartTime` / `Duration` | 手工精确编排（前摇 0.2s → 命中 0.3s → 后摇） |
| `Sequence` | 忽略绝对时间，按数组顺序首尾相接 | 多段连击、连锁触发 |
| `Parallel` | 忽略绝对时间，全部同时开始 | 伤害 + 特效 + 音效同时发生 |

配合轨道的 `StartDelay` / `RepeatCount` / `RepeatInterval`：

```
RepeatCount = 3, RepeatInterval = 0.5, 模式 = Sequence
→ 这一串片段依次跑完 → 隔 0.5s → 再跑一遍 → 再隔 0.5s → 再跑一遍
```

`Timeline` 模式下不支持重复（绝对时间重复会互相覆盖），执行计划里强制 `RepeatCount = 1`。

## 3. 执行计划的展开

`UAKAbilityAsset::BuildExecutionPlan()` 把"轨道模式 + 重复"展开成扁平的 `TArray<FAKScheduledTask>`：

```
轨道 A（Sequence, Repeat=2）  →  [A0 0.0-0.2][A1 0.2-0.5] [A0 0.5-0.7][A1 0.7-1.0]
轨道 B（Timeline）            →  [B0 0.3-0.35]
→ 排序后：[A0][A1][B0][A0][A1]
```

执行器只认这张表，因此**运行时没有任何"复合"概念**——复杂度被吸收在展开这一步。
编辑器也用同一张表绘制，保证"看到的即跑出来的"。

## 4. WaitUntil 与门控

`UAKAbilityTask_WaitUntil`：条件满足即继续，或超时后继续。

- 它默认 `bGateTimeline = true`：**存活期间整个时间轴的时间不前进**（持续任务仍会 Tick）。
- 实现：执行器每帧先 `IsGated()`，为真则跳过 `CurrentTime += Δt`。
- 提前结束：任务实现 `ShouldEndEarly()`，条件满足时执行器立即结束它，门控随之解除。

| 语义 | 配置 |
|---|---|
| 纯等待（Delay） | 条件留空 + `Timeout` = 等待时长 |
| 等条件 | 配置条件 + `Timeout` 作为兜底 |
| 等条件且不阻塞时间轴 | `bGateTimeline = false` |

校验器会拦截"既无条件也无超时"的配置（那会永久冻结时间轴）。

## 5. 与 Branch 的关系

| | Branch | WaitUntil |
|---|---|---|
| 回答 | 条件成立时**换一条分支** | 条件成立时**继续往下走** |
| 机制 | 写 `Context.DisabledTracks` 开关轨道 | 冻结/恢复时间轴推进 |
| 共用 | 两者都用同一个 `FAKCondition`（Tag / 属性），语义一致 | |

## 6. 已知限制

- Repeat 的间隔是固定值，不支持"每次间隔不同"（需要时用多个 Sequence 轨道）。
- `Sequence` 模式下单帧任务（Duration=0）会挤在同一时刻，按数组顺序执行——这是有意的。
- 被 Repeat 复制出来的任务在主时间轴上是同一批对象，因此 `ScratchPad` 会按对象复用；
  若任务需要"每次重复独立状态"，请在 `OnTaskEnd` 里重置自己的 ScratchPad。
