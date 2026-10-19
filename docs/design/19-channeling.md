# 引导 / 吟唱设计（已定稿，M5）

## 1. 它是什么，不是什么

| | 语义 | 触发 |
|---|---|---|
| **Channeling（引导）** | 条件**不满足**就打断 | 维持一件事成立（蓄力、站桩、锁定） |
| `WaitUntil`（等待） | 条件**满足**就继续 | 等一件事发生 |
| `Branch`（分支） | 单次判定开关轨道 | 进入时判定一次 |

三者共用同一套 `FAKCondition`，但**时机与失败语义完全不同**——这是容易混的地方。

## 2. 配置（`UAKAbilityAsset`）

| 字段 | 说明 |
|---|---|
| `bChanneled` | 是否为引导技能 |
| `ChannelingStartTime` / `ChannelingEndTime` | 引导窗口；`EndTime = -1` 表示到技能结束 |
| `ChannelingConditions` | 需要持续维持的条件（**任一**不满足即失败） |
| `ChannelFailureAction` | 失败时做什么 |
| `bEvaluateChannelingOnAuthorityOnly` | 是否只在权威端判定 |

失败动作：

| 值 | 行为 |
|---|---|
| `CancelAbility`（默认） | `Finish(true)` → 任务收到 `bInterrupted=true` → 技能被取消 |
| `FinishNormally` | `Finish(false)` → 视为引导完成 |
| `SkipToEnd` | 光标跳到末尾后 `Finish(false)`（跳过剩余编排） |

## 3. 条件类型

| 类型 | 判定 | 网络 |
|---|---|---|
| `Tag / 属性` | 复用 `FAKCondition`（与 Branch、WaitUntil 一致） | 不需要通知 |
| `移动速度` | 施法者速度 > 阈值即打断（站桩吟唱） | 不需要 |
| `距离` | 与第一个目标距离 > 阈值即打断（锁定技） | 不需要 |
| `自定义` | 蓝图 / C++ 子类实现 | **需要通知服务器**（`bRequiresServerNotification`） |

**输入为什么不内置**：按键绑定是项目相关的，把输入系统钉进框架会限制使用方式。
做法是 `UAKChanneling_CustomCondition` + 蓝图子类，在里面读输入并声明需要通知服务器。

## 4. 执行时机

```
Tick(Δt)
├─ 时间推进（受门控影响）
├─ 【引导检查】在窗口内 && 本端该判定 && 有任一条件不满足 → ApplyChannelingFailure() 并 return
├─ StartDueTasks / EndTasksOnDisabledTracks / TickActiveTasks / EndDueTasks
└─ 完成或循环判定
```

引导检查放在**任务推进之前**：条件一旦失败，本帧不再启动任何新任务，避免出现"打断瞬间又放出一个伤害"。

## 5. 与 GAS 的关系

- 引导**不自建状态**：不搞 "ChannelingComponent"，只是每帧读 GAS 的 Tag / 属性与 Actor 的位置速度。
- 打断走标准链路：`Finish(true)` → `OnFinished` → `UGameplayAbility::EndAbility(cancelled)`，
  因此 GAS 的预测回滚照常生效。
- 需要"引导中"这种状态给 UI 或被动用？用 **GE 授予 Tag**（例如 `State.Channeling`），
  这是 GAS 的做法，UAK 不额外提供。

## 6. 已知限制

- 引导条件目前**不参与 Json 序列化**（`ChannelingConditions` 是内联对象，需要按 `$type` 展开，M5 后续补）。
  导入 Json 后请重新配置引导条件，校验器会给出提示。
- 客户端不会本地预判引导失败（`bEvaluateChannelingOnAuthorityOnly` 默认 true），
  因此网络延迟下打断会有一帧左右的滞后。需要即时手感时，用 Custom 条件 + Cue 在客户端先停表现。
- 引导失败不会回滚已施加的 GE（与中断一致，回滚归 `FPredictionKey` 管）。
