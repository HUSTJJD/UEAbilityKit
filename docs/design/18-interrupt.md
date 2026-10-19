# 打断传播语义（已定稿，M5）

## 1. 为什么要把"打断"显式化

多段技能被打断时，"先停哪个、哪些必须做完"直接决定两件事：
- 表现是否突兀（特效还在放、位移突然截断）
- 关键逻辑会不会丢（收尾 GE 没落地、资源没回收）

Able 的中断是"遍历活跃列表逐个结束"，顺序由遍历决定——不可控。
UAK 把它显式化为三条规则。

## 2. 规则一：结束顺序（`EAKInterruptOrder`）

| 值 | 语义 | 适用 |
|---|---|---|
| `ReverseStartOrder`（默认） | 后启动的先结束（栈式回退） | 大多数技能：回退到"打断前那一刻" |
| `StartOrder` | 按启动顺序结束 | 需要保证前段先收尾的链式技能 |
| `TrackPriority` | 先停表现，再停逻辑，最后停主时序 | 表现丰富的技能，避免特效残留 |

`TrackPriority` 的顺序：
`Presentation(0) → Motion(1) → Query(2) → Effect(3) → Custom(4) → Rule(5) → Main(6)`

实现上依赖 `FAKActiveTask::StartOrder`，因此 `EndDueTasks` 用 `RemoveAt` 而非 `RemoveAtSwap`
——保序是中断顺序正确性的前提（这是一处容易被忽略的坑）。

## 3. 规则二：轨道被停用 = 取消，不是中断

Branch 把某条轨道停用后，该轨道上**已经在跑**的任务会被立即结束，且带 `bInterrupted = true`：

- 语义上是"这个分支被取消了"，而不是"整个技能被打断"；
- 由 `EndTasksOnDisabledTracks()` 每帧检查，因此分支生效的下一帧就会收敛；
- 会在 Trace 上记一条"所属轨道被 Branch 停用而取消"，调试时能区分两种结束原因。

注意 `DisabledTracks` 只影响**尚未启动**的任务（在 `StartDueTasks` 里过滤）与**正在运行**的任务（上面这条），
已完成的单帧任务不会被回溯。

## 4. 规则三：保证执行（`bAlwaysExecute`）

任务可标记"即使被中断也保证至少执行一次"：

| 情况 | 行为 |
|---|---|
| 中断时正在运行 | 以 `bInterrupted = false` 结束（正常收尾，不突兀） |
| 中断时还没轮到 | **补执行一次** `OnTaskStart` → `OnTaskEnd(false)` |
| 所属轨道已被 Branch 停用 | 跳过（分支明确关闭了它，不违背分支语义） |
| 本端不该执行（NetMode） | 跳过 |

典型用途：必中的收尾 GE、资源回收、必须发出的关键 Cue。

Trace 上会记"技能被中断，但标记为保证执行，补执行一次"，避免事后误判为漏配。

## 5. 门控与中断

`WaitUntil` 这类门控任务在中断时必须解除门控，否则时间轴会永久冻结。
由于 `Finish()` 会清空 `ActiveTasks`，门控必然随之解除——
但这一点容易在后续改动中被破坏，因此实现上把"清空活跃列表"放在 `Finish` 的**第一步**之后立即执行。

## 6. 循环重启不是中断

`RestartLoop()` 结束存活任务时传 `bInterrupted = false`（正常结束），
因为循环是技能自身的正常流程，不是被打断。它同样遵守 `InterruptOrder`。

## 7. 中断来源

| 来源 | 路径 |
|---|---|
| 技能被取消 / 打断 | GAS `CancelAbility` → `EndAbility(bWasCancelled=true)` → `Finish(true)` |
| 技能正常结束 | `EndAbility` → `Finish(bWasCancelled)` |
| 时间轴跑完 | `Tick` → `Finish(false)` |
| 分支停用轨道 | `EndTasksOnDisabledTracks()` → 单任务 `OnTaskEnd(true)` |

## 8. 已知限制

- 中断不做"回滚"：已经施加的 GE 不会撤销（GAS 的回滚由 `FPredictionKey` 负责，UAK 不介入）。
- `bAlwaysExecute` 的任务若自身又依赖前序结果（例如依赖 Query 的目标），补执行时可能拿不到目标——
  建议这类任务自带兜底（如 `ApplyGameplayEffect` 的 `bApplyToSelfIfNoTarget`）。
- 复合轨道被打断时，Repeat 剩余轮次直接丢弃（不补跑）。
