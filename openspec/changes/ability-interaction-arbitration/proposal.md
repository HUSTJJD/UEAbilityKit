## Why

UAK 现在的技能是**各自正确**的，但没有**共同正确**的保证，更没有**规模上正确**的保证。

一个角色身上同时挂着多少个技能的效果，不由任何一个技能决定：A 技能加了 `Stun`，B 技能也加了 `Stun`，A 结束把 Tag 摘掉，B 还以为自己生效着；两个减速同时来，30% + 40% 直接叠成 70%；眩晕和击飞同时命中，谁生效取决于执行顺序而不是设计意图。资产上只有 GAS 原生的四个容器（`AbilityTags` / `CancelAbilitiesWithTag` / `BlockAbilitiesWithTag` / `RequiredTags`）——那是**技能级**互斥（能不能激活），不是**状态级**仲裁（生效时谁赢）。

这件事在小规模下靠测试能兜住，在目标规模下兜不住，而且**失效方式不止一种**：

- **正确性维度**：组合数是技能数的阶乘级，穷举不完，也看不过来。多角色预览只能让人"看见一次冲突"，不能让系统"保证不冲突"。
- **规模维度（大型多人 MMO）**：数百上千可施放实体共存于一个区域，服务器权威，DS 每帧预算以毫秒计、每玩家带宽以 KB/s 计。当前"每帧 Tick 每个实例"的模型、以及"复制交给 GAS 就行"的假设，在这个量级下都会崩。
- **强度维度（高强度 boss 战）**：单个目标身上并发上百层状态，每秒数百次状态申请，外加阶段切换导致的状态集合整体变更。当前单点、每帧、按 GE 复制的路径无法承受。

所以要补的不是可视化，也不是补丁式的计数规则，而是一套**分层框架**：把技能之间隐式的相互影响变成显式的、可配置的、可被机器检查的契约，同时让这套契约在服务器权威、大规模、高频的场景下仍然跑得动。

UAK 现在的位置适合承载这套框架——技能是纯数据资产、能双向 Json、有 `Validate()` 自校验、有 `UAKTrace` 溯源、有 commandlet 门禁。缺的是**分层**：数据、结算、复制、观测、治理必须是五层，而不是全部塞进"任务直接调 ASC"。

## What Changes

**L1 数据层**
- 技能定义与 `UAsset` 分离：编辑器产出资产（权威、可 diff），构建期编译为紧凑运行时格式，支持流式加载与**运行时热重载**（不停机改数值）。
- 状态域配置（`UAKStatusDomainConfig`）成为可热更新的版本化数据资产。

**L2 结算层**
- 引入**固定步长 + 事件驱动调度**：施放实例在无事件时休眠（零 Tick），到期由全局时间轮唤醒；实例对象池化，高频路径无分配。
- 引入**状态仲裁器**：技能提交申请而非直接改状态；裁决结果记录到**归属账本**（哪层来自谁）。
- 引入**批量结算**：多目标场景（AOE / boss 战）走批量路径，共享规则查找与账本访问。
- 支持**阶段操作**：boss 阶段切换时的状态集合整体变更与规则覆盖。

**L3 复制层**
- 引入**复制视图**：服务器持有完整账本（含归属，永不复制），客户端只接收**精简、按相关性过滤、增量、量化**的状态摘要。引入 AOI / 相关性 / LOD。
- 明确与 GAS 的分工：属性仍由 GE 计算与复制，UAK 只复制"客户端需要展示的状态摘要"。

**L4 观测层**
- Trace 分级采样（规模下不全量记录），支持战斗录像与离线回放。
- 编辑器展示"仲裁后"的结果与抑制关系。

**L5 治理层**
- 不变量 + 组合 fuzz（正确性）。
- **离线仿真**：headless 批量重跑战斗，产出状态快照与统计，用于回归与平衡分析。
- 静态冲突扫描支持**增量**与索引持久化，作为 CI 门禁。
- 全系统**预算与降级**策略（CPU / 带宽 / 更新频率），超预算时按既定顺序降级而非失控。

**BREAKING**：无。现有技能资产与任务行为保持不变；新能力通过新增任务类型与显式开关启用。内部执行调度改为事件驱动，但对外时序语义保持等价。

## Capabilities

### New Capabilities

- `status-domain-config`：状态域与仲裁规则的数据资产层——定义哪些效果互相干扰、优先级、聚合、上限、免疫，含版本化与热重载。
- `status-arbitration`：运行时仲裁核心——申请 / 裁决 / 归属账本 / 生命周期，含批量路径、阶段操作、无分配热路径与实例池化。
- `owned-tag-lifecycle`：Tag 的归属与引用计数——解决共享 Tag 被无关技能提前摘除，含向后兼容增强与断线重建。
- `effect-aggregation`：属性类效果的聚合、衰减与上限——减速取最强而非相加、连续控制递减、总效果钳制；使同类 GE 数量恒定以控制复制与属性聚合开销。
- `fixed-step-scheduling`：固定步长时钟、事件驱动调度、实例休眠与对象池化——让上千并发施放不再每帧 Tick。
- `batch-resolution`：多目标批量结算——AOE / boss 战的批量申请、批量裁决与批量落地。
- `replication-view`：复制视图与相关性——服务器完整账本与客户端精简视图分离，AOI / LOD / 增量 / 量化。
- `hot-reload`：技能定义与域配置的运行时热更新——版本化、迁移策略、正在生效状态的安全迁移。
- `budget-degradation`：全系统预算与降级——CPU / 带宽 / 更新频率预算，超预算时按固定顺序降级。
- `simulation-regression`：离线仿真与回归——headless 批量重跑战斗，产出快照与统计，支持差异比对。
- `arbitration-observability`：仲裁可观测性——Trace 分级采样、编辑器视图、调试命令、录像与回放。
- `invariant-testing`：不变量与组合 fuzz——以系统级性质替代组合穷举。
- `conflict-scan`：静态冲突扫描与 CI 门禁——增量扫描、索引持久化、基线豁免。

### Modified Capabilities

无。仓库此前没有 `openspec/specs/`；现有行为定义在 `docs/design/*.md` 中，本次以新增能力的方式扩展，不修改既有需求。

## Impact

**新增代码**（`Plugins/UEAbilityKit/Source/`）
- Runtime：`Public/Status/`（`UAKStatusDomainConfig`、`UAKStatusResolver`、`FAKStatusLedger`、`UAKStatusReplicationView`）、`Public/Scheduling/`（固定步长时钟、时间轮、实例池）、新增 `UAKAbilityTask_ApplyStatus`。
- Runtime 改动：`UAKAbilityInstance` 改为可休眠与池化；`UAKAbilityTask_ApplyLooseTags` 增加归属计数（默认关闭）；`FAKAbilityContext` 增加仲裁器与调度器访问入口；`UAKTrace` 增加裁决节点与采样分级。
- Editor：GAS 影响条与血缘视图展示仲裁结果；`SUAKDebuggerPanel` 增加状态账本与复制视图页；`UAKValidateCommandlet` 增加 `-scan-conflicts`（支持增量）与 `-simulate`。
- Tests：新增 `UAK.Status.*`、`UAK.Invariant.*`、`UAK.Scheduling.*`、`UAK.Replication.*` 等用例组。

**宿主工程**：零改动。全部能力在插件内完成，宿主不必修改其 `UGameplayAbility` / ASC / 角色基类。

**依赖**：无新增第三方依赖。

**纪律影响**：`docs/design/13-network.md` 的"UAK 不自建网络同步"**继续成立但需细化**——属性计算与复制仍由 GAS 负责；UAK 新增的是"状态摘要的复制视图"，它不是状态的真值（真值在服务器账本与 ASC），只是给客户端看的投影。本 change 在 `design.md` 中明确记录这一边界如何被满足。

**文档**：新增 `docs/design/21-status-arbitration.md`（框架总览）与 `docs/design/22-scale-and-budget.md`（规模与预算），`docs/roadmap.md` 增加 M6 分组。
