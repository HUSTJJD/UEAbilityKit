# 基于 UE5 + GAS 的开源技能编辑器：调研、命名与仓库方案

> 调研对象：① 商业 UE 插件 **Able**（本地 `.../Gameplay/Able`、`AbleDebugger`）；② 开源 **AbilityKit**（`github.com/HOBOBO/AbilityKit`）
> 目标：两者取优 + 原生 GAS + 不重复造轮子 + 规避 Able 版权风险

---

## 0. 一句话结论

**AbilityKit 是 Unity / 纯 C# 项目，不是 UE 插件**——它的代码一行都不能直接搬，值得学的是它的**分层架构与机制设计**（Pipeline / Triggering / Trace / Continuous / 配置可回导）。
**Able 是 UE 商业插件（Extra Life Studios，Marketplace 销售）**——值得学的是它的**编辑器 UX 与时间轴 Task 模型**，且它**不使用 GAS**（只用 `FGameplayTagContainer`），网络同步很弱。

因此我们的定位是明确的空白地带：

> **UE5 原生 + GAS 一等公民 + 可 diff 的数据资产 + 时间轴可视化编排 + 运行时可调试**
> 即：**用 Able 的编辑器体验，承载 AbilityKit 的架构思想，跑在 GAS 之上。**

---

## 1. Able 调研（UE 商业插件）

### 1.1 核心数据模型

| 概念 | 说明 | 证据 |
|---|---|---|
| `UAblAbility` | 一条"时间轴技能"资产（Blueprintable）：Length / Cooldown / PlayRate / IsBullet、被动与堆叠（MaxStacks、Refresh 策略）、Loop（起止+最大迭代）、`m_Tasks[]` + UID、依赖缓存、Realm、ECA 模块数组、仅编辑器的预览角色/武器/子弹/缩略图字段 | `AbleCore/Classes/ablAbility.h:434-524`，预览字段 `392-449` |
| `UAblAbilityTask` | `EditInlineNew`、无状态。契约：`CanStart / OnTaskStart / OnTaskTick / IsDone / OnTaskEnd`，另有 `NeedsTick`、`IsSingleFrame`、`IsAsyncFriendly`、`GetTaskRealm`(Client/Server/Both)、依赖表、`CreateScratchPad`（状态外置）、`GetStatId`、`OnAbilityTimeSet`（编辑器拖时间轴回调） | `AbleCore/Classes/Tasks/IAblAbilityTask.h:93-150` |
| 时间驱动 | **纯秒级**：`CanStart` = 当前时间 ≥ StartTime，`IsDone` = 当前时间 ≥ EndTime；帧只在编辑器 UI 层做对齐 | `AbleCore/Private/Tasks/IAblAbilityTask.cpp:47-50, 72-75` |
| 目标 / 碰撞 | `EAblAbilityTargetType`(Self/Owner/Instigator/TargetActor/Camera)；碰撞查询 `FAblQueryResult`，支持 `FTraceHandle` 异步 | `ablAbilityContext.h:18-68` |
| 分支 | `UAblBranchTask + UAblBranchCondition`(OnInput/Always/Custom，可 Negate) | `Tasks/ablBranchTask.h:86-107` |

### 1.2 运行链路

`Ability(配置) → Context(单次运行数据：Owner/Instigator/Targets/Stack/Time/ScratchPad/异步句柄) → Instance(执行器，不复制 Task，只存 Sync/Async 活动列表，Fire&Forget) → Component(驱动器)`

- `UAblAbilityComponent`（`UActorComponent + IGameplayTagAssetInterface`，`TG_DuringPhysics`）**单 Tick 串行驱动**：冷却 → Pending/Branch 队列 → 异步目标查询回收 → `InternalUpdateAbility(DeltaTime * PlayRate)` → 被动 → 权威端复制。
  `AbleCore/Private/ablAbilityComponent.cpp:43-188`
- 所谓 "Async" 只是把冷却/实例更新丢到 TaskGraph，目标线程仍是 GameThread，**不是真正并行**（`ablAbilityUtilities.h:16-56`）。
- 网络只同步"当前激活 Ability + 被动列表"（按 AbilityNameHash 匹配）。

### 1.3 编辑器组成（最值得借鉴的部分）

布局（`ablAbilityEditorModes.cpp:28-83`）：工具栏 / 左侧 Ability 属性 / 右上视口 + 右下时间轴 / 右侧 Task 详情 / Editor Settings，Graph、Timeline 双模式。

| 能力 | 实现 | 借鉴价值 |
|---|---|---|
| 自绘时间轴与轨道（拖首尾、整体拖动、网格） | `SAbilityTimeline.h:16-90`、`SAbilityTimelineTrack.h:14-136` | ★★★ 核心 |
| 预览视口 + 缩略图抓取 | `SAbilityViewport.h`、`AblAbilityThumbnailRenderer.h` | ★★★ |
| 预览角色/武器 + 动画选择器（可一键按动画长度裁剪技能） | `ablAbilityEditor.h:159-195`、`SAbilityAnimationSelector.h` | ★★★ |
| Task Picker（树形分类 + 搜索 + 支持 BP Task） | `SAbilityTaskPicker.h:54-131` | ★★★ |
| 验证器（按 Task 类绑定、TaskGraph 并行、Log/Warning/Error 分级） | `AblAbilityTaskValidator.h:99-160` | ★★★ |
| Json/Binary 导入导出（按 Gameplay/Effect/Sound × Realm 分文件、可加密） | `AblJsonSerializer.h:36-116` | ★★★ |
| Task/ECA 复制粘贴、Git 提交 Json | `ablAbilityEditor.h:109-115, 257-279` | ★★ |
| 单帧步进、播放/暂停、吸附/网格/字体 Settings | `ablAbilityEditor.h:171`、`ablSettings.h:46-71` | ★★ |
| 对外扩展点 `FAbleEditorEventManager` | `AbleEditorEventManager.h:11-68` | ★★ |
| **ECA 机制**（Event/Condition 单槽 + ActionTrue/False/Both 三组 Task，可整体启停/折叠/着色/拷贝/缩放时间） | `ablAbilityECAModule.h:14-197` | ★★★ 相当于"轨道内子图 + 语义分支" |

`AbleDebugger`：编辑器内运行时可视化（非网络协议）。控制台 `Able.Debug ENABLE/DISABLE/SUMMARY/HELP`，单例 `UAbleDebuggerEnterpoint`(FTickableEditorObject) 每帧刷新，UMG 列表 + 世界内 Billboard 显示当前 Ability/Task/ECA 活跃状态。

### 1.4 与 GAS 的关系（关键结论）

**Able 核心完全不用 GAS**，只用了 `FGameplayTagContainer / IGameplayTagAssetInterface`；冷却、堆叠、被动、打断全部自建。
GAS 桥接发生在**项目层**（你仓库里的 `MoeAblAbility` / `MoeAblAbilityTask` / `AblPlayAbilityMontageTask` 持 `UAbilitySystemComponent` + Montage + GameplayCue；攻击检测用 `FCollisionShape` + Tag 过滤）。

### 1.5 Able 的短板（= 我们的机会）

1. 非 GAS 原生 → 属性、GE、预测、Cue、Tag 无内在关联，每个项目都要写一遍桥接层。
2. 网络只同步"当前技能指针"，**无预测/回滚，Task 级结果不复制**。
3. uasset 二进制不可 diff → 被迫外置 Json + Git 双轨，维护成本高。
4. 时间按秒存储、帧只在 UI 对齐 → 难以做确定性/帧锁定。
5. 蓝图/Lua 依赖重（`IAblAbilityTask.h:253-268` 有 Lua 开关），缺纯 C++ 数据资产路径，性能与调试受限。
6. OrderValue / UID / 依赖表 / Realm 多套体系并存，概念冗余。
7. 通用 Task 层已被项目代码侵蚀（Cue/GE/Projectile/攻击检测/Lua 全部下沉到项目模块），复用性差。

---

## 2. AbilityKit 调研（Unity / 纯 C#，MIT，278 stars）

### 2.1 它是什么

面向中大型战斗项目的**可组合工具集**（不是单一产品包）：以 Unity UPM 包组织，核心战斗逻辑是**纯 C#**，可脱离 Unity 跑在服务器、控制台、测试环境；Unity 侧只承担入口、创作工具与表现投影。**MIT 协议**。

### 2.2 三层边界（最值得抄的思想）

```
宿主层（Unity Root / Console / ET / Orleans）
   ↓
项目策略层（组合根、配置规范、系统顺序、权威模型、表现投影）  ← 框架不决定
   ↓
工具集机制层（Core / Pipeline / Triggering / Combat / World / Sync / Network / Tooling）
```

框架只稳定"机制"，不强行抽象一套所有游戏都要实现的 BattleApplication。

### 2.3 可直接映射到 UE 的机制

| AbilityKit 模块 | 职责 | 我们在 UE/GAS 里的对应 |
|---|---|---|
| `pipeline` | Phase 图编排：Sequence/Parallel/Conditional/Repeat/Delay/WaitUntil/Timeline、暂停/中断/Trace | 时间轴轨道 + 复合轨道（UE 侧可用 Task 组合实现，不必另建 Phase 图） |
| `triggering` | EventBus + TriggerRunner（phase→priority→order）+ 条件（Payload/Blackboard/NumericExpr）+ 强类型 Action Schema + ExecutionControl | **ECA 轨道 + Trigger 规则系统**（对标 Able 的 ECA，但用强类型 Action） |
| `actionschema` | Action/Timeline 的强类型 DTO，配置 → 运行时 | `USTRUCT` + `Instanced` UObject Task，**参数强类型、可反射序列化** |
| `trace` | 溯源树：root/parent/owner context，技能→投射物→区域→Buff→伤害血缘 | 轻量 `UAKTrace`，供调试/伤害归因/回放 |
| `continuous` | 持续效果的条件化激活/阻止/暂停/恢复/移除 | **直接用 `UGameplayEffect`（Infinite/Duration）+ Tag 规则**，不自建 |
| `modifiers` | 参数修正器（Buff/装备/天赋改写技能参数） | **直接用 GE 的 Modifier / SetByCaller**，不自建 |
| `gameplaytags` / `attributes` | 标签与属性 | **直接用 `FGameplayTag` / `UAttributeSet`**，不自建 |
| `ability.explain` / `diagnostics` | 解释树、Profiler、结构化证据导出 | 调试面板 + Trace 导出（对标 AbleDebugger） |
| `world.* / network.* / framesync / statesync / record` | 帧同步、快照、状态同步、预测回滚 | **不自建**——GAS 自带预测 + ASC 复制 + `FPredictionKey`；DS 场景用 UE 网络 |
| `analyzer` (Roslyn) | 静态分析与项目约束检查 | 编辑器 Validator + 命令行 CI 校验（对标 Able Validator） |

### 2.4 不能照搬的部分

- 它是 C#/Unity，`world.ecs / entitas / svelto / Orleans / Luban` 与 UE 无关。
- 它的同步体系（帧同步/回滚）在 UE 里应让位给 GAS + UE 网络，否则就是重复造轮子。
- 它的 `combat.motion / navigation / collision` 在 UE 里直接用 `UCharacterMovementComponent`、`UNavigationSystemV1`、物理查询即可。

---

## 3. 两者取优：设计取舍表

| 维度 | 采用 Able 的 | 采用 AbilityKit 的 | 交给 GAS / UE 原生（不造轮子） |
|---|---|---|---|
| 资产形态 | 时间轴 + 轨道 UX、预览视口 | 强类型 Action Schema、配置可回导 | `UDataAsset`（可文本化导出，可 diff） |
| Task 模型 | `EditInlineNew` 无状态 Task + ScratchPad | 注册中心 + 强类型参数 + 可验证 | `UObject` 派生（C++ 为主，允许 BP 扩展） |
| 分支 | ECA 轨道分组（Event/Condition/Action） | Triggering（事件→条件→动作→执行控制） | Tag / GE 条件复用 |
| 属性 / Buff / 修正 | — | 机制思想 | `UAttributeSet` + `UGameplayEffect` + Modifier |
| 表现解耦 | Montage / 特效直接播 | Cue 回调 | **`UGameplayCueNotify`（GAS 原生）** |
| 冷却 / 消耗 / 堆叠 | 自建（弃） | 自建（弃） | **GE Cooldown / Cost / Stacking** |
| 网络同步 | 只同步指针（弃） | 帧同步/回滚（暂缓） | **ASC 复制 + PredictionKey + GE 复制** |
| 时间 | 秒 + 编辑器帧对齐 | 定点/确定性 | 秒为主，保留"帧锁定模式"开关 |
| 目标选择 | TargetType + 查询 | Targeting 框架（候选/筛选/评分/选择） | `UAbilityTask_WaitTargetData` + 自定义 `FGameplayAbilityTargetDataHandle` |
| 校验 | Validator（并行、分级） | Analyzer + 自动化测试 | 编辑器 Validator + 命令行 CI |
| 调试 | AbleDebugger（UMG + Billboard） | explain / diagnostics / Trace | 运行时 Debugger + Trace 导出 |
| 序列化 | Json/Binary 导入导出 | 可读配置 ↔ 运行时双向 | Json + `.uasset` 双写，Json 进 Git |

**核心差异化一句话**：别人要么"编辑器好用但不基于 GAS"（Able），要么"架构严谨但不是 UE"（AbilityKit）——我们做**UE 原生 GAS + 时间轴可视化 + 数据可 diff + 运行时可溯源**的那一档。

---

## 4. 命名提案

### 命名约束（按优先级重排）
1. **一眼可知**：看到仓库名就知道是 UE 的技能编辑器（用户核心诉求）
2. 不与 Able / AbilityKit / GAS 官方术语撞名（规避版权与 SEO 混淆）
3. GitHub / 中文社区可搜索，尽量无撞车
4. 能生成干净好记的 C++ 前缀（UE 模块必需）

### 候选（直白型优先）

| # | 名称 | 中文称呼 | 一眼可知 | 品牌感 | 风险 | 备注 |
|---|---|---|---|---|---|---|
| **1 ⭐** | **`UEAbilityKit`** | **UE 技能工具箱 / UE 技能编辑器** | ★★★★★ | ★★★ | 低 | UE + Ability + Kit 三要素齐备；"Kit" 能装下 runtime + editor + debugger 整套 |
| 2 | `UEAbilityEditor` | UE 技能编辑器 | ★★★★★ | ★★ | 低 | 更精准指向"编辑器"，但未来装 runtime 框架时略窄 |
| 3 | `UEAbilityStudio` | UE 技能工作台 | ★★★★☆ | ★★★ | 低 | 产品感强，"编辑器"意图略弱 |
| 4 | `UESkillEditor` | UE 技能编辑器 | ★★★★★ | ★★ | 低 | 中文最直觉，但与 UE 官方术语 **Ability** 不一致，海外传播吃亏 |
| 5 | `Baton`（早期方案） | 指挥棒 | ★☆☆☆☆ | ★★★★★ | 低 | 品牌最好，但看不出是 UE 技能编辑器 |
| ⚠️ | `UnrealAbilityKit`（全拼） | 虚幻技能工具箱 | ★★★★★ | ★★★ | **中** | "Unreal®" 是 Epic 注册商标；且与 HOBOBO/AbilityKit（Unity）易混淆 |

### 商标与撞名核查（针对 "Unreal Ability Kit"）

| 检查项 | 结论 |
|---|---|
| GitHub 撞名 | 检索 `UnrealAbilityKit` / `UEAbilityKit` / `Unreal Ability Kit` 未发现已存在的同类 UE 插件仓库（落地前建议在 GitHub 再确认一次组织名与仓库名可用性） |
| **Epic 商标** | "Unreal®" 是 Epic Games 注册商标。把自己的产品命名为 **"Unreal XXX"** 有"暗示官方出品/背书"的风险；Epic 的商标许可针对 Logo 与品牌素材审批，社区通行的低风险做法是 **nominative use**：用 `UE` / `UE5` 缩写，或写成 "XXX for Unreal Engine"。另外不得使用 Epic Logo、官方字体与配色 |
| **与 AbilityKit 混淆** | HOBOBO/AbilityKit 是 **Unity / 纯 C#** 项目。直接叫 "Unreal Ability Kit" 极易被理解为它的 UE 移植版或姊妹项目，带来归属与认知问题；加 `UE` 前缀 + `NOTICE` 声明"独立实现、仅参考公开设计思想、无代码复用"可基本规避 |

### 推荐：**UEAbilityKit**（简称 UAK）

- **保留了你要的"Ability Kit"品牌感**，同时用 `UE` 前缀压掉商标风险、并和 Unity 版 AbilityKit 明确区分。
- **一眼可知**：UE + Ability + Kit，中文传播语就写 **"UE 技能编辑器"**，把"编辑器"这个核心卖点锚在副标题上。
- **语义容量更大**：`Kit` 能装下 runtime（资产 + 执行器 + GAS 桥接 + Trace）、editor（时间轴）、debugger 三块；叫 `Editor` 则未来扩展运行时框架时名字会变窄。
- **不与 Able 混淆**：与 `Able` 无字面相似。

> 若你更想强调"这就是个编辑器"而非"工具集"，用 **`UEAbilityEditor`**（简称 UAE）——它与 GAS 的 `UGameplayAbility` 术语也更贴。

### 命名落地（以 UEAbilityKit 为例）

| 项 | 值 |
|---|---|
| 仓库名 | `UEAbilityKit`（GitHub） |
| 展示名 / 简称 | `UAK` |
| 中文名 | **UE 技能编辑器**（传播语） / UE 技能工具箱（全称） |
| README 首行 | `UAK — UE5 技能编辑器：基于 GAS 的数据驱动技能编排工具集` |
| uplugin / 插件目录 | `UEAbilityKit`（`Plugins/UEAbilityKit/`） |
| 运行时模块 | `UEAbilityKit`（`UEABILITYKIT_API`）—— UE 惯例：runtime 模块与插件同名 |
| 编辑器模块 | `UEAbilityKitEditor`（`UEABILITYKITEDITOR_API`） |
| 调试模块 | `UEAbilityKitDebugger`（可选，或并入 Editor） |
| **C++ 类前缀** | **`AK`**（`U` + `AKAbilityAsset` = `UAKAbilityAsset`）：`UAKAbilityAsset`、`UAKAbilityTask`、`UAKGameplayAbility`、`FAKContext`、`UAKTrace` |
| 目录 | `Plugins/UEAbilityKit/Source/UEAbilityKit/...` |

> 命名取舍说明：仓库/插件名要"一眼可知"所以直白；C++ 前缀用 `AK` 保持类名短、避免 `UUAK...` 双字母丑名且不撞引擎符号，二者不要求字面一致，这是 UE 项目的常见做法。
> 合规提醒：README 与 `NOTICE` 中写明「Unreal® 是 Epic Games, Inc. 的商标；本项目为社区独立项目，非 Epic 官方产品」，不使用 Epic 官方 Logo/字体。

---

## 5. 仓库与插件骨架建议

```
UEAbilityKit/                        # 仓库根（GitHub 仓库名同为 UEAbilityKit）
├─ README.md                      # 首行：UAK — UE5 技能编辑器（基于 GAS）
├─ LICENSE                        # 建议 Apache-2.0（含专利声明）；要最大传播用 MIT
├─ .gitignore / .gitattributes    # *.uasset 走 LFS；Json 走文本
├─ NOTICE                         # 声明：设计参考 AbilityKit(MIT) 与 Able 的公开交互模式，无代码复用
├─ docs/
│  ├─ research/                   # 本次调研（Able / AbilityKit）
│  ├─ design/                     # canonical 设计：资产模型、Task 契约、时间模型、Trace、同步
│  └─ roadmap.md
├─ Plugins/UEAbilityKit/
│  ├─ UEAbilityKit.uplugin
│  ├─ Resources/  Content/
│  └─ Source/
│     ├─ UEAbilityKit/            # Runtime（不依赖 Editor）
│     │   ├─ Assets/              # UAKAbilityAsset / UAKAbilitySet
│     │   ├─ Task/                # UAKAbilityTaskBase + 内置 Task（Montage/GE/Cue/Collision/Motion/Branch/Trigger）
│     │   ├─ Track/               # 轨道与复合轨道（Sequence/Parallel/ECA）
│     │   ├─ Exec/                # UAKAbilityInstance / UAKContext / ScratchPad
│     │   ├─ Gas/                 # UAKGameplayAbility（UGameplayAbility 桥接）/ TargetData / ASC 扩展
│     │   ├─ Trace/               # UAKTrace 溯源
│     │   └─ Serialize/           # Json 导入导出
│     ├─ UEAbilityKitEditor/      # Editor（时间轴、视口、Picker、Validator、缩略图）
│     └─ UEAbilityKitTests/       # 自动化（可选）
└─ tools/                         # CI 校验脚本
```

`UEAbilityKit.uplugin` 依赖建议：`GameplayAbilities`、`GameplayTags`、`GameplayTasks`、`EnhancedInput`（可选）、`Slate/SlateCore/UnrealEd`（Editor 模块）。

---

## 6. 版权规避要点

1. **不阅读/复制 Able 源码到新库**：只保留"公开可观察的交互模式"（时间轴 UX、验证器、预览视口等通用做法）。
2. 不使用 `abl` / `Abl` 前缀，不使用 Able 的 `Content/*.uasset`、`*.png` 资源与文档文案。
3. 不与 Able 的 Task 类同名同结构（重新设计 Task 契约，例如把 `CanStart/IsDone` 改为显式的 `Execute/Tick` + 时间区间数据）。
4. AbilityKit 是 **MIT**，可合法参考其设计文档与思想；在 `NOTICE` 中署名致谢即可（它代码是 C#，本身也不会被复制）。
5. 建议 **Apache-2.0**（比 MIT 多专利条款，适合被商业项目采用）；若目标是最大传播则用 MIT。

---

## 7. 里程碑建议

| 阶段 | 目标 | 产出 |
|---|---|---|
| M0 | 仓库 + 插件骨架 + 最小资产模型 | `UEAbilityKit.uplugin`、`UAKAbilityAsset`、空时间轴编辑器 |
| M1 | 运行时执行器 + GAS 桥接 | `UAKGameplayAbility` 驱动资产；GE / Cue / Tag 三类 Task |
| M2 | 编辑器可用 | 轨道拖拽、预览视口、Montage 对齐、单帧步进 |
| M3 | 治理能力 | Validator、Json 导入导出（进 Git）、CI 校验 |
| M4 | 调试闭环 | Runtime Debugger（对标 AbleDebugger）+ Trace 导出 |
| M5 | 高级编排 | ECA/Trigger 轨道、分支、复合轨道、目标选择与碰撞 Task |

---

## 8. 需要你确认的 3 件事

1. **名字**：定 `UEAbilityKit`（简称 UAK），还是 `UEAbilityEditor`（UAE）/ `UnrealAbilityKit`（全拼，商标风险中）？
2. **UE 版本**：仓库基础版本（5.3 / 5.4 / 5.5）？我看到本地工程指向内部引擎分支 `ue4_tracking_rdcsp`，插件里声明了 `OpenHarmony` 平台，需要你给一个对外开源的目标版本。
3. **License**：Apache-2.0（推荐）还是 MIT？

确认后我可以一次性落地：`README` / `LICENSE` / `NOTICE` / `UEAbilityKit.uplugin` / 三个模块的 `Build.cs` + 最小类骨架（`UAKAbilityAsset`、`UAKAbilityTaskBase`、`UAKGameplayAbility`）+ `.gitignore`，直接可编译。
