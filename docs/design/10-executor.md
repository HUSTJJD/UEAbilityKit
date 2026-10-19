# 执行器设计（已定稿，M1）

## 1. 职责边界

`UAKAbilityInstance` 只负责把**一条资产**推进成**一次施放**：时间推进、任务进入/结束、循环、中断。

不负责：目标选择（交给 TargetData 与 Query 任务）、表现（交给 Cue）、网络（交给 GAS）、属性（交给 AttributeSet）。

## 2. 驱动方式：AbilityTask

```
UAKGameplayAbility::ActivateAbility
  ├─ NewObject<UAKAbilityInstance>
  └─ UAbilityTask::NewAbilityTask<UAKAbilityTickTask>
        └─ TickTask(Δt) → UAKAbilityInstance::Tick(Δt)
```

**为什么选它**（在 UE 5.8.3 源码中核对过 `UGameplayTask::bTickingTask` 为 protected、`TickTask` 为虚函数、`UAbilityTask::NewAbilityTask<T>()` 存在）：

| 方案 | 问题 |
|---|---|
| `FTSTicker` | 需要自己管理注销，技能被取消时易泄漏；生命周期与技能不同步 |
| 独立组件 | 要在角色上挂组件，侵入项目结构 |
| **AbilityTask** ✅ | 技能结束 → 任务自动结束 → 时间轴自动收尾；天然处于 GAS 能力作用域内 |

## 3. 每帧流程

```
Tick(Δt)
├─ CurrentTime += Δt * PlayRate
├─ StartDueTasks()   起始时间 <= CurrentTime 的任务进入；单帧任务进入即结束
├─ TickActiveTasks() 仅 bNeedsTick 的持续任务
├─ EndDueTasks()     结束时间 <= CurrentTime 的任务退出
└─ 完成判定：CurrentTime >= Length → 循环(RestartLoop) 或 Finish(false)
```

任务在 `Initialize` 时按 `StartTime` 升序排好，运行时用 `PendingIndex` 单向推进，不做重复扫描。

## 4. 三个过滤（都在 StartDueTasks 内）

| 过滤 | 依据 | 目的 |
|---|---|---|
| 轨道停用 | `Context.DisabledTracks`（由 Branch 任务写入） | 条件分支 |
| 执行端 | `EAKTaskNetMode` + `Context.bIsAuthority/bIsLocallyControlled` | 避免表现任务在权威端空跑 |
| 轨道开关 | `Track->bEnabled`（`CollectTasks` 时过滤） | 整轨静音 |

## 5. 状态与 GC

- Task 无状态，运行时状态放 `Context.ScratchPads`（`TMap<Task*, UObject*>`）。
- `Context` 是 `UAKAbilityInstance` 的 `UPROPERTY` 成员，因此 ScratchPad 能被 GC 正确追踪。
- 从 Task 反查所属轨道：`Cast<UAKAbilityTrack>(Task->GetOuter())`（Task 是轨道的 Instanced 子对象）。

## 6. 中断语义

| 场景 | 调用 | 任务收到 |
|---|---|---|
| 时间轴自然跑完 | `Finish(false)` | `OnTaskEnd(bInterrupted=false)` |
| 技能被取消/打断 | `EndAbility` → `Finish(true)` | `OnTaskEnd(bInterrupted=true)` |
| 循环重启 | `RestartLoop()` | `OnTaskEnd(false)`，然后重新武装队列 |

## 7. 已知限制

- 当前不支持"任务内等待外部信号"（WaitUntil）；M5 的复合轨道会补。
- 循环重启会重新触发 `StartTime <= LoopStartTime` 的任务，这是有意的（循环段内的任务每轮都跑）。
