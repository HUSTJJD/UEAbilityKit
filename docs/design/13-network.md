# 网络设计（已定稿）

## 1. 核心立场：UAK 不自建网络同步

这是与 AbilityKit 最大的分歧点。AbilityKit 自带帧同步 / 快照 / 状态同步 / 回滚，
因为它是 Unity 项目，那些能力无处可借。在 UE 里这些能力**已经存在且更好**：

| 需求 | UE/GAS 的既有方案 | UAK 是否会自建 |
|---|---|---|
| 技能激活与结束 | `UAbilitySystemComponent` 的 RPC + `FPredictionKey` | 否 |
| 属性与 GE 复制 | ASC 的属性复制、`FActiveGameplayEffectsContainer` | 否 |
| 客户端预测与回滚 | GAS 的 Prediction Key / 属性回滚 | 否 |
| 目标数据 | `FGameplayAbilityTargetDataHandle` + 服务器确认流程 | 否，只做适配 |
| 表现 | `UGameplayCueNotify`（自带复制与预测处理） | 否 |
| 蒙太奇 | `ASC->PlayMontage`（自带复制与打断） | 否 |
| 帧同步 / 快照 / 回滚 | 项目自选（UE 网络层 / 第三方） | 否，UAK 不介入 |

**结论**：UAK 的运行时逻辑层**一行网络代码都不写**。
网络正确性来自"所有副作用都落在 GAS 原语上"这一条纪律。

## 2. UAK 与网络有关的唯一机制：`EAKTaskNetMode`

它**不是**同步机制，只是"哪一端应该执行"的标记，避免表现类任务在权威端空跑：

| 值 | 含义 | 判定 |
|---|---|---|
| `Both` | 两端都跑（默认） | — |
| `ServerOnly` | 只权威端跑 | `Context.bIsAuthority` |
| `ClientOnly` | 只本地端跑 | `Context.bIsLocallyControlled` |

典型用法：`PlayMontage`、`SendGameplayCue` 默认 `ClientOnly`。

## 3. 项目需要注意的边界

- 权威端仍会执行 `ServerOnly` 与 `Both` 的任务，因此**任务不得依赖本地表现**。
- 若某个任务既要逻辑又要表现，请拆成两个任务（一个 `Both` 走 GE，一个 `ClientOnly` 走 Cue）。
- 冷却 / 消耗 / 堆叠走 GE，因此天然可复制——请不要用资产的 `PlayRate` 之类去做权威判定。

## 4. 待补（M5）

- 文档化"某个技能在不同 `NetExecutionPolicy` 下的行为差异"。
- 提供一套最小可跑的多人示例（ASC on PlayerState + LocalPredicted 技能）。
