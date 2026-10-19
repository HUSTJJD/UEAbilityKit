# 资产模型设计

**设计目标**：把 AbilityKit 的"强类型 Action + 可回导配置 + 溯源"思想，装进 Able 那种"一条时间轴描述一次技能"的形态里，
并且**资产本身不持有技能逻辑**——逻辑归 GAS，资产只描述"时间轴上发生什么"。

---

## 1. 核心决策：资产与 Ability 解耦（N:M）

```
UAKGameplayAbility（激活方式 / 冷却 / 消耗 / 目标 / 网络策略）  ←→  UAKAbilityAsset（时间轴数据）
      1                                                                   1
      ┆  N : M                                                            ┆
```

- `UAKAbilityAsset`：**只描述时间轴上发生什么**（纯数据）。
- `UAKGameplayAbility`：**如何被激活**、冷却（`CooldownGE`）、消耗（`CostGE`）、目标获取方式、网络执行策略。

**收益**（这是 Able 与 AbilityKit 都不具备的）：

| 场景 | 做法 |
|---|---|
| 同一套动作，不同冷却 / 消耗 / 输入 | 一个资产 + 多个 `UAKGameplayAbility` |
| 同一技能，不同武器 / 形态变体 | 一个 `UAKGameplayAbility` 按 Tag 选择不同资产 |
| 换皮、批量调数值 | 只改资产，不动 Ability |
| 服务器批量校验 | 资产是纯数据，可脱离 Actor 加载与校验 |

> Able 的 Ability 与资产是一体的（Blueprint 资产）；AbilityKit 的配置与技能运行时绑定更紧。
> 解耦后，资产可以被引擎外的工具（CI、配表工具）直接消费。

---

## 2. 两级结构：Track（轨道）→ Task（片段）

```
UAKAbilityAsset
└─ Tracks[] : UAKAbilityTrack          # 容器：分组 / 类型 / 颜色 / 开关 / 折叠
     └─ Tasks[] : UAKAbilityTaskBase   # 片段：StartTime / Duration / 强类型参数
```

- **Track 是一级容器**（`UObject`，非 Task）：它决定"这一类事情"的整体开关与呈现，
  例如「表现」轨道可整体静音，只留逻辑轨道跑服务器校验。
- **Task 是轨道上的片段**：自带 `StartTime` / `Duration` 与强类型参数（沿用 M0 已落地的 `UAKAbilityTaskBase`）。

### 2.1 `UAKAbilityAsset` 字段

| 分组 | 字段 | 说明 |
|---|---|---|
| Meta | `DisplayName` / `Category` / `Version` / `Author` / `Description` | 资产元信息，`Version` 用于 Json 升级 |
| Timing | `Length` / `PlayRate` / `FrameRate`（仅编辑器对齐，默认 60）/ `bLoop` / `LoopRange` | 时间以**秒**存储 |
| Tags | `AbilityTags` / `CancelAbilitiesWithTag` / `BlockAbilitiesWithTag` / `RequiredTags` / `CooldownTag` | 全部直接喂给 GAS |
| Tracks | `Tracks : TArray<UAKAbilityTrack*>`（`Instanced`） | 两级结构的根 |
| Markers | `Markers : TArray<FAKTimeMarker>` | 命名时间点（`Windup`/`Cast`/`Recover`），供 Trigger 与时间轴引用 |
| Preview | `PreviewCharacterClass` / `PreviewMontage` / `PreviewWeapon` | **仅编辑器**（`WITH_EDITORONLY_DATA`） |

> 冷却与消耗**不放资产**：它们属于 Ability 的激活策略（GE），放在 `UAKGameplayAbility` 上。

### 2.2 `UAKAbilityTrack` 字段

| 字段 | 说明 |
|---|---|
| `TrackName` | 显示名 |
| `TrackType` | 见下表，决定默认颜色、可接受的 Task 类型、绘制方式 |
| `bEnabled` / `bMute` / `bCollapsed` | 整体开关，便于"只跑逻辑"或"只看表现" |
| `Tasks` | `Instanced` 的 Task 列表 |

| TrackType | 用途 | 典型 Task |
|---|---|---|
| `Main` | 主时序（动画 / 阶段） | PlayMontage、TimeMarker |
| `Effect` | 逻辑效果 | ApplyGameplayEffect、RemoveGameplayEffect |
| `Presentation` | 表现（客户端） | GameplayCue、CameraShake、Sound |
| `Query` | 目标与碰撞 | CollisionQuery、Targeting |
| `Motion` | 位移 | Dash、MotionWarping |
| `Rule` | 事件规则（M5） | Trigger、Branch |
| `Custom` | 项目自定义 | 任意 |

---

## 3. Task：强类型、自带校验、自带 GAS 影响描述

每个 Task 子类除了业务参数，还实现三个契约（**这是 UAK 的原创点**，Able 的 Validator 是外部绑定的）：

```cpp
// 1) 生命周期（M0 已有）
virtual void OnTaskStart(FAKAbilityContext&) {}
virtual void OnTaskTick(FAKAbilityContext&, float) {}
virtual void OnTaskEnd(FAKAbilityContext&, bool bInterrupted) {}

// 2) 自校验：让错误"无法被配置出来"，而不是事后报错（M3）
virtual void Validate(const FAKValidationContext&, FAKValidationResult&) const {}

// 3) 声明自己会对 GAS 产生什么影响（M2，供 GAS 影响条与血缘视图使用）
virtual void GetGASImpact(FAKGASImpact& Out) const {}
```

`GetGASImpact` 的输出示例（编辑器据此实时渲染，无需运行游戏）：

```
ApplyGE(GE_Damage) @0.30s
  → 施加 GE: GE_Damage (Instant)
  → 影响属性: Health -{SetByCaller:Damage}
  → 触发 Cue: GameplayCue.Fire.Hit
  → 依赖 Tag: 无
```

### 内置 Task 清单（按 AbilityKit 的"原子能力"思路划分，落点全在 GAS）

| 类别 | Task | GAS 落点 |
|---|---|---|
| 时序 | `PlayMontage` / `TimeMarker` | AnimMontage（逻辑不依赖动画） |
| 效果 | `ApplyGameplayEffect` / `RemoveGameplayEffect` | `UGameplayEffect` / `FActiveGameplayEffectHandle` |
| 查询 | `CollisionQuery`（Shape / Channel / Tag 过滤）/ `Targeting` | `FGameplayAbilityTargetDataHandle` |
| 表现 | `SendGameplayCue` / `CameraShake` / `PlaySound` | `UGameplayCueNotify` |
| 运动 | `Dash` / `MotionWarping` | `URootMotionModifier` / Motion Warping |
| 控制（M5） | `Branch`（Tag / Attribute 条件）/ `Trigger` | Tag Requirements / Attribute 查询 |
| 高级（M5） | `SpawnProjectile` / `SpawnAreaEffect` | GE + Cue 组合 |

---

## 4. 与 GAS 的完整映射（"不重复造轮子"的清单）

| UAK 概念 | GAS 落点 | 说明 |
|---|---|---|
| 技能资产 | 被 `UAKGameplayAbility` 引用 | N:M |
| 冷却 | `CooldownGameplayEffect` | 在 Ability 上 |
| 消耗 | `CostGameplayEffect` | 在 Ability 上 |
| 属性变化 / Buff / 护盾 | `UGameplayEffect` | Instant / Duration / Infinite |
| 持续效果的暂停 / 移除 | GE 的 Tag Requirements | **不自建 Continuous 运行时** |
| 参数动态修正 | `SetByCaller` / Attribute-based float / GE Modifier | **不自建修正器系统** |
| 状态与互斥 | `FGameplayTag` | Asset Tags + GE GrantedTags |
| 表现 | `UGameplayCueNotify` | 逻辑与表现严格分离 |
| 目标 | `FGameplayAbilityTargetDataHandle` | 复用 GAS 目标数据 |
| 网络 | `FPredictionKey` + ASC 复制 | **UAK 逻辑层不碰网络** |
| 条件判断 | GE Tag Requirements / Attribute 查询 | 不自研表达式引擎 |

---

## 5. 时间模型

- **存储单位：秒**（`float`）。与 GAS 的 Duration / Montage 时基一致。
- **编辑器按帧对齐**：`FrameRate`（默认 60，可配 30）只影响吸附与显示，不影响存储。
- **预留 `bFrameLocked`**（M5）：开启后所有时间量化到帧，为确定性与回放留设计位。
  Able 的时间只按秒存、帧只在 UI 对齐；我们显式保留这个开关。

---

## 6. 序列化（M3）：资产 ↔ Json 双向

设计要点（继承 AbilityKit "可读配置可回导" 的思想）：

- 只导出**逻辑相关**字段，`Preview*` 等 `WITH_EDITORONLY_DATA` 字段不导出。
- 每个 Task 用**强类型 schema**导出（而不是 Able 那种按属性表 dump），字段名稳定、可 review。
- 资产带 `Version`，导入时按版本升级（不静默失败）。
- 分文件策略：一个资产一个 `.json`（不按类型拆分，避免过度设计）。

```json
{
  "version": 1,
  "asset": "Fireball",
  "length": 1.5,
  "playRate": 1.0,
  "frameRate": 60,
  "tags": ["Ability.Fire", "Ability.Cast"],
  "tracks": [
    {
      "name": "Effect", "type": "Effect", "enabled": true,
      "tasks": [
        {
          "$type": "ApplyGameplayEffect",
          "startTime": 0.30, "duration": 0.0,
          "effectClass": "/Game/GE/GE_Damage.GE_Damage_C",
          "level": 1.0,
          "setByCaller": { "Damage": 120 }
        }
      ]
    }
  ]
}
```

---

## 7. 版本与兼容

- 资产 `Version` 字段 + Json `version` 字段双写，导入时按版本迁移。
- Task 的 `$type` 使用稳定类名；重命名时通过 `FormerNames` 做兼容（M3）。
- CI 校验（M3）会在 Json 变更时跑全量 `Validate()`，把"配错"挡在合入之前。
