# 设计文档索引

> canonical 入口：本目录记录 UAK 的能力边界、所有权归属、生命周期与已知限制。
> 涉及精确 API 与成熟度时，以本目录文档 + 当前源码为准，README 只负责能力认知与接入起点。

## 已定稿（M0）

| 主题 | 结论 |
|---|---|
| 资产模型 | `UAKAbilityAsset : UPrimaryDataAsset`，字段：Length / PlayRate / bLoop / AbilityTags / Tasks（`Instanced` 内联子对象） |
| Task 契约 | `UAKAbilityTaskBase`：`EditInlineNew` + `DefaultToInstanced`、**无状态**、`StartTime` / `Duration` / `bNeedsTick`，生命周期 `OnTaskStart → OnTaskTick → OnTaskEnd`，运行时状态经 `CreateScratchPad` 外置 |
| GAS 桥接 | `UAKGameplayAbility : UGameplayAbility` 负责驱动资产；冷却/消耗/堆叠/目标数据/预测/复制全部交给 GAS |
| C++ 命名 | 模块 `UEAbilityKit`（Runtime）/ `UEAbilityKitEditor`（Editor）/ `UEAbilityKitDebugger`（M4）；类前缀 `AK`：`UAKAbilityAsset`、`UAKAbilityTaskBase`、`FAKAbilityContext`、`UAKTrace` |
| 版权边界 | 不复用 Able 源码/资源/文案，不使用 `abl` 前缀；AbilityKit（MIT）仅参考设计思想，见 `NOTICE` |
| 验证引擎 | UE **5.8.3**（源码版）。已核对的 API：`UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AActor*)`、`UAbilitySystemComponent::MakeOutgoingSpec(...)` / `ApplyGameplayEffectSpecToTarget(const FGameplayEffectSpec&, UASC*, FPredictionKey)`、`UGameplayAbility::ActivateAbility/EndAbility`、`FGameplayAbilityActorInfo::{OwnerActor, AvatarActor, AbilitySystemComponent}`、`IAssetTypeActions::{GetName, GetSupportedClass, GetTypeColor}` |

## 必读

- [01-design-principles.md](01-design-principles.md) — **从 AbilityKit 学到的精髓、与 Able 的刻意差异化、UAK 的原创点**。
  动手写任何模块前先读它：它定义了"什么该做、什么坚决不做"。

## 已定稿（设计）

- [02-asset-model.md](02-asset-model.md) — 资产模型：**资产与 Ability 解耦（N:M）**、Track→Task 两级结构、
  Task 三契约（生命周期 / `Validate` / `GetGASImpact`）、与 GAS 的完整映射、时间模型、Json schema 草案
- [03-editor-design.md](03-editor-design.md) — 编辑器：**三视图（时间轴 / 血缘 / 校验）**、布局、
  与 Able 的逐处对照（通用 / 刻意不同 / 独创）、GAS 影响条、血缘视图、交互约定

## 已定稿（机制）

| 主题 | 文档 | 关键结论 |
|---|---|---|
| 执行器 | [10-executor.md](10-executor.md) | `UAKAbilityInstance` + `UAKAbilityTickTask`（AbilityTask 驱动）；三个过滤（轨道停用 / 执行端 / 轨道开关） |
| 目标选择 | [11-targeting.md](11-targeting.md) | 不自建目标选择系统，只把 GAS 的 TargetData 适配进 Context |
| 溯源 | [12-trace.md](12-trace.md) | `ParentIndex` 构树；关闭时零开销；PIE 联动切实况 |
| 网络 | [13-network.md](13-network.md) | **UAK 逻辑层一行网络代码都不写**，全部落在 GAS 原语上 |
| 序列化 | [14-serialization.md](14-serialization.md) | Json 双向；`$type` 定位类；只导出逻辑字段 |
| 验证 | [15-validation.md](15-validation.md) | Task 自带 `Validate()`（内聚）+ 常驻底栏 + CI 门禁 |
| 复合编排 | [16-composite.md](16-composite.md) | 组合语义放在**轨道**上（Sequence / Parallel / Repeat）；`WaitUntil` 门控冻结时间轴 |
| 投射物与区域 | [17-projectile-and-area.md](17-projectile-and-area.md) | 只负责"飞、撞到谁、把 GE+Cue 打上去"；持续与周期交给 GE，物理交给引擎 |
| 打断传播 | [18-interrupt.md](18-interrupt.md) | 三条规则：结束顺序 `EAKInterruptOrder` / 轨道停用=取消 / `bAlwaysExecute` 保证执行 |
| 引导 / 吟唱 | [19-channeling.md](19-channeling.md) | 条件**不满足**就打断；与 WaitUntil（满足才继续）语义相反；失败动作三种 |
| 脚本集成 | [20-scripting-integration.md](20-scripting-integration.md) | UAK 核心不依赖任何脚本系统，**示例模块也不依赖**（绑定走 ini 映射）；`.uplugin` 引用标 `Optional`；扩展点只有两个（数据驱动对象绑定 / 条件转发）；脚本打包需显式 stage；`Tools/unluanova.ps1 setup\|doctor` 负责自检 |

## 待定稿（M5 及以后）

| 主题 | 说明 |
|---|---|
| 多人示例 | ASC on PlayerState + LocalPredicted 的最小可跑示例 |
| 引导条件序列化 | `ChannelingConditions` 暂未进 Json（需按 `$type` 展开内联对象） |
| 规则集扩展 | 让项目能注册自己的校验规则 |
| 投射物的客户端预测 | 当前不做本地预生成，需要时评估用 Cue 补手感 |

## 不属于本项目解决的问题

- 帧同步 / 回滚 / 快照：交给项目与 UE 网络层；UAK 只保证自己的逻辑可被 GAS 预测与复制。
- 移动、寻路、碰撞求解：复用引擎已有能力（`UCharacterMovementComponent`、`UNavigationSystemV1`、物理查询）。
- 属性、Buff、修正器公式：复用 `UAttributeSet` 与 `UGameplayEffect`。
