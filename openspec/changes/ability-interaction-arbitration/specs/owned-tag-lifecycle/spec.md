## ADDED Requirements

### Requirement: 归属计数为可选增强且默认关闭

`UAKAbilityTask_ApplyLooseTags` SHALL 新增归属计数开关（默认关闭）。关闭时该任务的行为 MUST 与变更前逐字节一致；开启时其产生的 Tag 受归属账本管理。

理由：存量技能不能因为升级插件而改变行为。归属是一次**显式选择**，不是全局强制。

#### Scenario: 默认行为不变

- **GIVEN** 一个未启用归属计数的 `ApplyLooseTags` 任务
- **WHEN** 两个技能先后施加与移除同一个 Tag
- **THEN** 行为与变更前完全一致：任一技能结束时都会摘除该 Tag

#### Scenario: 启用后按归属摘除

- **GIVEN** 两个 `ApplyLooseTags` 任务都启用了归属计数，施加同一个 `Ability.Casting`
- **WHEN** 其中一个任务结束
- **THEN** 只有该任务对应的层被移除，另一个任务的 Tag 仍然生效

### Requirement: 归属者由施放实例与任务标识构成

归属 Key SHALL 由"本次施放的 `UAKAbilityInstance`"与"任务的稳定标识（`TaskId`）"共同构成。系统 MUST 区分同一资产的多次施放。

#### Scenario: 同一技能两次施放互不干扰

- **GIVEN** 同一角色在 0.0s 与 0.5s 各施放一次同一技能，两次都申请 `Ability.Casting` 1.0s
- **WHEN** 第一次施放在 1.0s 结束
- **THEN** 第二次施放的层仍存活至 1.5s，目标在 1.0s~1.5s 期间仍拥有该 Tag

#### Scenario: 缺少任务标识时报错

- **GIVEN** 一个启用了归属计数的 `ApplyLooseTags` 任务，`TaskId` 为空
- **WHEN** 资产校验运行
- **THEN** 报告一条 Error：启用归属计数必须填写 `TaskId`，否则无法区分同一任务的多次施放

### Requirement: Tag 的实际增删仍通过 ASC 落地

归属账本 SHALL 只负责决定"何时该增删"，实际的 Tag 增删 MUST 通过 `UAbilitySystemComponent` 完成（`AddLooseGameplayTags` / `RemoveLooseGameplayTags`）。系统 MUST NOT 维护一份脱离 ASC 的"影子 Tag 状态"。

理由：只有走 ASC，Tag 的复制、预测与回滚才由 GAS 负责，这是 UAK 不自建网络的既定纪律。账本是可重建的视图，不是真值。

#### Scenario: 账本与 ASC 状态一致

- **GIVEN** 目标身上有归属 A 与归属 B 两层同一 Tag
- **WHEN** 归属 A 释放
- **THEN** ASC 上该 Tag 仍然存在（层数仍为正），且账本与 ASC 的实际状态一致

#### Scenario: 外部改动可被感知

- **GIVEN** 目标身上有归属 A 的一层 Tag
- **WHEN** 外部系统（非 UAK）通过 ASC 直接移除了该 Tag
- **THEN** 账本在下一次裁决时检测到不一致并修正自身，不产生"幽灵层"

### Requirement: 归属层随施放结束强制释放

当一次施放结束时，系统 SHALL 释放该施放申请的全部 Tag 层，且 MUST 在以下所有路径上都执行：正常结束、被中断、被取消、门控解除、Branch 停用轨道、角色销毁、世界切换。

#### Scenario: 角色销毁时不泄漏

- **GIVEN** 某角色身上有归属 A 的状态层
- **WHEN** 该角色被销毁
- **THEN** 账本中与该角色相关的全部层被清理，不残留悬挂引用

#### Scenario: 轨道被停用时释放

- **GIVEN** 一个 `ApplyLooseTags` 任务位于 Rule 轨道上，且该轨道被 `Branch` 停用
- **WHEN** 轨道停用生效
- **THEN** 该任务申请的状态层被释放，目标状态按剩余层重新裁决

### Requirement: 中断遗留由可控策略决定

`ApplyLooseTags` 的既有 `bRemoveOnInterrupt` SHALL 继续生效，但在启用归属计数后其语义 MUST 明确为"该层是否在中断时立即释放"；未启用时该层由归属者生命周期决定。两者 MUST NOT 产生歧义行为。

#### Scenario: 中断时立即释放

- **GIVEN** 启用归属计数且 `bRemoveOnInterrupt == true`
- **WHEN** 施放被中断
- **THEN** 该层立即释放，不等待其自然到期

#### Scenario: 中断时保留至自然到期

- **GIVEN** 启用归属计数且 `bRemoveOnInterrupt == false`
- **WHEN** 施放被中断
- **THEN** 该层保留至其自然到期时间，到期后由系统释放，并在校验时给出 Info 提示"存在中断后残留的路径"
