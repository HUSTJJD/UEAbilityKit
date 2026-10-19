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

## 待定稿（M1–M5，按里程碑补写）

| 主题 | 计划文档 | 说明 |
|---|---|---|
| 执行器 | `10-executor.md` | `UAKAbilityInstance` 的推进方式（每帧驱动 vs AbilityTask / Ticker）、中断与循环语义 |
| 目标选择 | `11-targeting.md` | 复用 `FGameplayAbilityTargetDataHandle` 与目标选择 Task |
| 溯源 | `12-trace.md` | root/parent/owner 血缘，供调试、伤害归因与回放 |
| 网络 | `13-network.md` | 明确依赖 GAS 预测 + ASC 复制，本项目不自建同步 |
| 序列化 | `14-serialization.md` | Json 双向导出、版本升级、CI 校验 |
| 验证 | `15-validation.md` | `Validate()` 规则集与命令行校验 |

## 不属于本项目解决的问题

- 帧同步 / 回滚 / 快照：交给项目与 UE 网络层；UAK 只保证自己的逻辑可被 GAS 预测与复制。
- 移动、寻路、碰撞求解：复用引擎已有能力（`UCharacterMovementComponent`、`UNavigationSystemV1`、物理查询）。
- 属性、Buff、修正器公式：复用 `UAttributeSet` 与 `UGameplayEffect`。
