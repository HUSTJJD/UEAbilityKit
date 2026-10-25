## ADDED Requirements

### Requirement: 状态域作为数据资产

系统 SHALL 以数据资产（`UAKStatusDomainConfig`）定义"状态域"（Status Domain），每个域描述一组会互相干扰的效果及其仲裁规则。域的增删改 MUST NOT 需要修改 C++ 代码或重新编译。

理由：上万技能的规模下，域的数量与规则会随玩法持续演进；若域是代码枚举，每次新增都要改代码、重编译、并让所有宿主工程同步升级，这在规模上是不可接受的。

#### Scenario: 新增一个状态域无需改代码

- **GIVEN** 工程中已存在状态域配置资产，其中包含 `CC.Stun`
- **WHEN** 策划在该资产中新增一个域 `CC.Fear`（优先级低于 `CC.Stun`，聚合方式为 `Strongest`，上限 1）
- **THEN** 重新启动编辑器后 `CC.Fear` 即可被技能申请与仲裁，无需修改任何 C++ 代码

#### Scenario: 域未登记时给出显式错误

- **GIVEN** 某技能申请了状态 `CC.Petrify`
- **WHEN** 配置中不存在覆盖 `CC.Petrify` 的域
- **THEN** 资产校验（`UAKAbilityAsset::Validate`）报告一条 Error："状态 CC.Petrify 未登记到任何状态域"，且该技能在运行时不产生任何状态效果

### Requirement: 域定义包含四类仲裁规则

每个状态域 SHALL 且 MUST 完整定义以下四类规则，缺项 MUST 在校验时报错：

1. **优先级（Priority）**：整数，值大者胜；用于跨域或同域不同状态的压制关系。
2. **聚合方式（Aggregation）**：`Strongest`（取最强，其余抑制）/ `Stack`（按层叠加，各层独立过期）/ `Refresh`（刷新时长，不叠加）/ `Diminish`（衰减叠加）。
3. **上限（Limit）**：同域同时生效的层数上限或强度上限；超出部分按聚合方式处理。
4. **免疫关系（Immunity）**：以 GameplayTag 表达的免疫声明，命中则申请被拒绝。

#### Scenario: 同域取最强

- **GIVEN** 域 `CC.Slow` 的聚合方式为 `Strongest`
- **WHEN** 目标身上已有来源 A 的 30% 减速，此时来源 B 申请 40% 减速
- **THEN** 生效值为 40%（不是 70%），且 B 的层被标记为生效层、A 的层被标记为抑制层

#### Scenario: 被抑制层在压制者结束后恢复

- **GIVEN** 目标身上有 A(30%) 与 B(40%) 两层减速，B 生效、A 被抑制
- **WHEN** B 的层到期被移除
- **THEN** 生效值自动回落到 A 的 30%，无需 A 重新申请

#### Scenario: 上限钳制

- **GIVEN** 域 `CC.Slow` 的强度上限为 60%
- **WHEN** 三个来源分别申请 40% / 30% / 50% 减速
- **THEN** 生效值为 60%（受上限钳制），而不是取最强的 50% 或叠加后的 120%

#### Scenario: 免疫拒绝申请

- **GIVEN** 域 `CC.Stun` 声明免疫 Tag 为 `Immune.CC.Stun`，且目标 ASC 当前拥有该 Tag
- **WHEN** 某技能向该目标申请 `CC.Stun`
- **THEN** 申请被拒绝，目标状态不变，且该拒绝被写入 Trace 并带原因 `Immune`

### Requirement: 域与 Tag 命名空间绑定

每个域 SHALL 通过 GameplayTag 前缀绑定到一组状态 Tag（例如域 `CC.Hard` 绑定 `State.CC.Stun`、`State.CC.KnockUp`）。系统 MUST 利用 Tag 的层级关系判定归属：一个状态 Tag 只属于**最具体**的那个匹配域。

#### Scenario: 子 Tag 归属于更具体的域

- **GIVEN** 域 `CC.Hard` 绑定前缀 `State.CC`，域 `CC.Stun` 绑定前缀 `State.CC.Stun`
- **WHEN** 系统解析状态 `State.CC.Stun.Heavy`
- **THEN** 该状态归属于 `CC.Stun`（更具体的匹配），不参与 `CC.Hard` 的仲裁

#### Scenario: 跨层不冲突

- **GIVEN** `State.CC.Stun` 属于域 `CC.Stun`，`Mark.Burning` 属于域 `Mark`
- **WHEN** 目标同时拥有这两个状态
- **THEN** 两者互不干扰，各自独立计算——不同域之间 MUST NOT 产生任何仲裁行为

### Requirement: 默认域保证向后兼容

系统 SHALL 提供一个默认域（优先级 0、聚合方式 `Stack`、无上限、无免疫），用于覆盖所有未显式登记的状态。命中默认域时 MUST 保持与当前 `ApplyLooseTags` 完全一致的行为。

理由：存量技能必须在不修改的前提下继续工作，否则迁移成本会让这套体系无法落地。

#### Scenario: 未登记状态沿用旧行为

- **GIVEN** 某技能使用 `UAKAbilityTask_ApplyLooseTags` 施加 `Ability.Casting`，且该 Tag 未登记到任何域
- **WHEN** 两个技能先后施加同一个 `Ability.Casting`
- **THEN** 行为与变更前完全一致（谁结束谁摘，不引入任何新的计数或仲裁）

### Requirement: 域配置可自校验

`UAKStatusDomainConfig` SHALL 实现自校验，并在以下情况报告 Error：优先级相同的两个域存在重叠的 Tag 前缀；聚合方式为 `Strongest` 但强度不可比较（未定义强度字段）；上限小于最小申请粒度；免疫 Tag 指向自身域。

#### Scenario: 检测到规则冲突

- **GIVEN** 两个域 `A` 与 `B` 绑定了相同的前缀 `State.CC` 且优先级相同
- **WHEN** 保存该配置资产或运行 `UAKValidateCommandlet`
- **THEN** 报告一条 Error，指出两个域的绑定重叠且优先级未区分

### Requirement: 配置携带版本号与迁移策略

状态域配置 SHALL 携带版本号，且每个域 MUST 声明热重载时的迁移策略（`Keep` / `Preserve` / `Expire`）。未声明时 MUST 使用 `Keep` 并报告 Warning。

#### Scenario: 版本随变更递增

- **GIVEN** 某域的上限被修改并保存
- **WHEN** 读取配置版本
- **THEN** 版本号递增，且可通过版本号判断两次运行是否使用同一套规则

#### Scenario: 缺失迁移策略时告警

- **GIVEN** 某域未声明迁移策略
- **WHEN** 保存配置或运行校验
- **THEN** 报告一条 Warning，指出该域应显式声明迁移策略

### Requirement: 支持阶段级规则覆盖

系统 SHALL 支持在全局规则之上叠加**阶段级覆盖**（如 boss 某阶段免疫硬控）。覆盖 MUST 具有明确的作用域（作用于哪些目标）与生命周期（何时失效），且 MUST 可被查询与撤销。

#### Scenario: 覆盖生效

- **GIVEN** 某 boss 进入阶段 2，该阶段覆盖 `CC.Stun` 为免疫
- **WHEN** 玩家施加眩晕
- **THEN** 申请被拒绝，且可查询到当前生效规则来源于"阶段覆盖"而非全局配置

#### Scenario: 覆盖失效后回到全局规则

- **GIVEN** 阶段 2 结束
- **WHEN** 再次查询 `CC.Stun` 规则
- **THEN** 生效规则回到全局配置，覆盖层被移除且不残留

#### Scenario: 覆盖可被显式撤销

- **GIVEN** 某阶段覆盖处于生效状态
- **WHEN** 显式撤销该覆盖（如 boss 被强制打断阶段）
- **THEN** 覆盖立即失效，目标状态按全局规则重新裁决
