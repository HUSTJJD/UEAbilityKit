# 冷却 / 消耗 / 堆叠示例

> 这三件事 **UAK 一行代码都不做**——它们全部是 GAS 的能力，UAK 只是不去挡路。
> 本文说明"该怎么配"，以及为什么不该在 UAK 资产里配。

## 1. 为什么不放进 `UAKAbilityAsset`

资产的定位是**一条时间轴**，且它与 Ability 是 **N:M** 关系：
同一条资产可以被多个技能（不同冷却/消耗）引用。
如果把冷却写进资产，这个解耦就被破坏了。

## 2. 冷却（`CooldownGameplayEffect`）

在 `UAKGameplayAbility` 上：

| 字段 | 值 |
|---|---|
| `Cooldown Gameplay Effect` | 指向一个 `GE_Cooldown_XXX` |

`GE_Cooldown_XXX` 的配置：

| 字段 | 值 |
|---|---|
| `Duration Policy` | `Has Duration` |
| `Duration Magnitude` | 例如 5.0 |
| `Granted Tags` | `Cooldown.Skill.XXX`（供 UI 查询剩余时间） |

查询剩余时间：

```cpp
float TimeRemaining = 0.f, Duration = 0.f;
Ability->GetCooldownTimeRemainingAndDuration(Handle, ActorInfo, TimeRemaining, Duration);
```

## 3. 消耗（`CostGameplayEffect`）

| 字段 | 值 |
|---|---|
| `Cost Gameplay Effect` | 指向 `GE_Cost_Mana_30` |

`GE_Cost_Mana_30` 的配置：

| 字段 | 值 |
|---|---|
| `Duration Policy` | `Instant` |
| `Modifiers` | `Attribute = Mana`，`Operation = Additive`，`Magnitude = -30` |

消耗不足时 GAS 会自动拒绝激活（`CommitAbility` 返回 false）。

## 4. 堆叠（Stacking）

Buff 类 GE 上配：

| 字段 | 值 |
|---|---|
| `Duration Policy` | `Has Duration` 或 `Infinite` |
| `Stacking Type` | `Aggregate by Source` / `Aggregate by Target` |
| `Stack Limit Count` | 例如 3 |
| `Stack Duration Refresh Policy` | `Refresh on Successful Application` |
| `Stack Expiration Policy` | `Clear Entire Stack` 或 `Remove Single Stack and Refresh Duration` |

## 5. 与 UAK 任务的配合

| 需求 | 做法 |
|---|---|
| 同一条时间轴、不同冷却 | 两个 `UAKGameplayAbility` 指向同一条资产 |
| 技能数值随等级变化 | 任务的 `SetByCaller` + 技能在计算 Spec 时写入 |
| 需要"引导中"这种状态 | 用 GE 授予 `State.Channeling` Tag，UAK 不额外提供状态 |
| 打断 | 用 `CancelAbilitiesWithTag` / `BlockAbilitiesWithTag`（资产上）或 GAS 的 `CancelAbility` |

## 6. 校验

`UAKValidateCommandlet` 不会检查 GE 配置（那是 GAS 的事），
但你可以把"技能没配冷却 GE"这类项目规则写进自己的校验规则里（见 15-validation 的"规则集扩展"）。
