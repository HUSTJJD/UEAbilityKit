# UAK — UE5 技能编辑器

**UEAbilityKit（简称 UAK）**：基于 Unreal Engine 5 与 Gameplay Ability System（GAS）的数据驱动技能编辑器与运行时工具集。

> 用时间轴编排技能：一段技能 = 一条时间轴 + 若干轨道（Montage / GameplayEffect / GameplayCue / 碰撞检测 / 位移 / 分支），
> 运行时全部落在 GAS 原语上（`UGameplayAbility` / `UGameplayEffect` / `UGameplayCue` / `FGameplayTag` / `UAttributeSet`），
> 不另起炉灶、不重复造轮子。

---

## 为什么做这个项目

现有方案各有短板，中间存在一块空白：

| 方案 | 优点 | 短板 |
|---|---|---|
| **Able**（UE 商业插件） | 时间轴编辑器体验成熟：轨道拖拽、预览视口、动画长度对齐、Task 验证器 | **不使用 GAS**（只用 `FGameplayTagContainer`），冷却/堆叠/被动全自建；网络只同步"当前技能指针"，无预测回滚；uasset 二进制不可 diff |
| **AbilityKit**（开源，Unity / 纯 C#，MIT） | 架构严谨：Pipeline 编排、Triggering（事件→条件→强类型 Action）、Trace 血缘溯源、配置可双向回导 | 是 **Unity / C#** 项目，代码无法用于 UE；其帧同步/回滚体系在 UE 里应让位给 GAS + UE 网络 |
| **裸写 GAS** | 官方、可预测、可复制 | 缺少可视化编排与调试工具，技能一多就靠表驱动硬扛 |

**UAK 的定位**：用 Able 级别的编辑器体验，承载 AbilityKit 级别的架构思想，跑在 GAS 之上。

## 设计原则

1. **GAS 一等公民**：属性、Buff、修正、冷却、消耗、堆叠、表现解耦、网络预测，一律复用 GAS 原语，不自建。
2. **不重复造轮子**：移动/寻路/碰撞用引擎已有能力；只做"技能编排"这一层真正缺失的东西。
3. **数据可 diff**：资产可导出为文本（Json）进 Git，CI 可校验，避免二进制黑盒。
4. **运行时可溯源**：技能 → 投射物 → 区域 → Buff → 伤害保留 root/parent/owner 血缘，便于调试、归因与回放。
5. **逻辑与表现分离**：逻辑走 GE / Tag / Task，表现一律走 `UGameplayCue`。
6. **C++ 优先，蓝图可扩展**：核心 Task 用 C++ `UObject` 实现（强类型、可反射、可验证），允许项目派生蓝图 Task。

## 当前状态

**当前进度：M0–M4 的主体代码已落地（尚未编译验证）**

**M0 — 骨架**
- 插件骨架：Runtime 模块 `UEAbilityKit` + Editor 模块 `UEAbilityKitEditor`
- `UAKAbilityAsset`（纯数据资产） + `UAKAbilityTrack` + `UAKAbilityTaskBase`

**M1 — 时间轴执行器 + GAS 桥接**
- 两级数据模型：资产 → 轨道 → 任务；**资产与 Ability 解耦（N:M）**
- `UAKAbilityInstance`：按时间轴推进，支持 Duration / 循环 / 中断 / PlayRate / ScratchPad
- 驱动方式：`UAKAbilityTickTask`（`UAbilityTask` 子类）——技能结束即自动收尾
- Task 三契约：生命周期 / `Validate()` / `GetGASImpact()` / `ExportJson·ImportJson`
- 内置 Task：`ApplyGameplayEffect`、`PlayMontage`、`SendGameplayCue`、`CollisionQuery`、`Dash`

**M2 — 编辑器（三视图）**
- `FUEAbilityKitAssetEditor`：Timeline / Inspector / Bloodline
- `SUAKTimeline` 自绘时间轴 + `SUAKGASImpactBar`（独创）+ `SUAKValidationBar`（常驻底栏）
- `SUAKBloodlineView` 血缘视图（独创）
- `UAKAbilityAssetFactory`：内容浏览器可创建 Ability Asset

**M3 — 治理**：`UAKAbilityJsonSerializer` 资产 ↔ Json 双向、Task 自校验

**M4 — 溯源**：`UAKTrace` 血缘树 + `UAKTraceSubsystem` 记录器

**M1/M3/M4 收尾**
- `UAKTargeting`：把 GAS 的 `FGameplayAbilityTargetDataHandle` 适配进上下文（不自建目标选择）
- `UAKValidateCommandlet`：CI 校验门禁（`-run=UAKValidate -path=/Game/Abilities -strict -json`）
- 执行器写入 `UAKTrace`（每次施放生成血缘树）

**M2 收尾**
- 预览 Tab（`FPreviewScene` + 预览角色 + Montage 播放/停止/循环）
- Task Picker（搜索 + 指定目标轨道 + 支持蓝图派生 Task）
- `UAKAbilityThumbnailRenderer`（内容浏览器画"技能结构示意图"）

**M4/M5 收尾**
- PIE 联动：时间轴光标跟随 + 血缘视图自动切实况 + PIE 期间自动开启记录
- Rule 轨道 `UAKAbilityTask_Branch`：用 GAS 的 Tag / 属性条件开关另一条轨道
- 设计文档补齐：`10-executor` / `11-targeting` / `12-trace` / `13-network` / `14-serialization` / `15-validation`

**M5 — 复合编排**
- 轨道级 `Sequence` / `Parallel` / 重复次数 / 重复间隔 / 起始延迟
- `BuildExecutionPlan()`：复合模式展开成扁平调度表，编辑器与运行时共用同一张表
- `UAKAbilityTask_WaitUntil`：条件满足即继续，`bGateTimeline` 可冻结时间轴
- `FAKCondition`：Branch 与 WaitUntil 共用同一套 GAS 条件语义

**M5 — 投射物与区域效果**
- `UAKQuery`：`CollisionQuery` / `AreaEffect` / 投射物共用同一份查询与过滤规则
- `AAKProjectile`：服务端权威、逐帧扫掠推进、命中打 GE + Cue、支持穿透/重力/超时
- `UAKAbilityTask_SpawnProjectile`：插槽或偏移生成、瞄准首个目标、扇形弹幕
- `UAKAbilityTask_AreaEffect`：周期生效、可跟随/固定中心、每目标只吃一次（ScratchPad 演示）

**M5 — 打断传播语义**
- `EAKInterruptOrder`：后启动先结束（默认，栈式回退）/ 顺序 / 轨道优先级
- 轨道被 Branch 停用 → 该轨道正在运行的任务按"取消"结束，与"技能被打断"区分
- `bAlwaysExecute`：即使被中断也保证执行一次（还没轮到就补执行）
- 门控任务在中断时必然解除，避免时间轴永久冻结

**M2 — 编辑器交互补全**
- 工具条：播放/暂停/回到起点/单帧步进、按帧显示、资产属性、+标记、从 Montage 生成
- 轨道管理（增删 + 上下移动）、任务管理（复制/删除）
- 快捷键：`空格` 播放暂停 · `←→` 单帧 · `Ctrl+D` 复制 · `Delete` 删除 · 滚轮缩放
- 从 Montage 生成：按长度设置技能长度 + 全长 PlayMontage 任务 + **Notify 转 Marker**

**M5 — 从实践中补进来的能力**
- **引导 / 吟唱（Channeling）**：窗口内维持条件，不满足即打断（蓄力 / 站桩 / 锁定）
- **TaskId**：同类任务的稳定标识符，血缘树里能区分"是哪个 ApplyGE 打的伤害"
- **Task 编辑器元数据**：分类 / 显示名 / 说明 / 配色，Task Picker 与时间轴直接使用
- **查询升级**：新增 Cone（锥形）形状；过滤器与形状解耦（Tag / 距离 / 视线）

**示例**

| 文档 / 目录 | 内容 |
|---|---|
| [`Samples/UAKSample`](Samples/UAKSample) | 可编译的源码示例插件：ASC + 属性集 + 带冷却/消耗的技能 |
| [`examples/01`](docs/examples/01-minimal-multiplayer-setup.md) | 最小多人接入（ASC 放哪、复制模式、NetMode 配对、常见坑） |
| [`examples/02`](docs/examples/02-cooldown-cost-stacking.md) | 冷却 / 消耗 / 堆叠该怎么配（全是 GAS，UAK 不自建） |

> 待完成：**编译验证**。功能、文档、示例均已完整，只差一次编译把报错修干净。

## 目录结构

```
UEAbilityKit/
├─ Plugins/UEAbilityKit/
│  ├─ UEAbilityKit.uplugin
│  └─ Source/
│     ├─ UEAbilityKit/            # Runtime（不依赖 Editor）
│     │   ├─ Public/  Private/
│     └─ UEAbilityKitEditor/      # Editor（M2 起承载时间轴编辑器）
├─ docs/
│  ├─ research/                   # Able / AbilityKit 调研与命名提案
│  ├─ design/                     # 设计文档（canonical 入口见 00-index.md）
│  └─ roadmap.md
├─ NOTICE
└─ LICENSE                        # Apache-2.0
```

## 快速开始

1. 把 `Plugins/UEAbilityKit` 整个目录拷贝到你的 UE 项目的 `Plugins/` 下。
2. 在 `.uproject` 的 `Plugins` 数组或通过编辑器启用 **UE Ability Kit** 插件，并确保 **GameplayAbilities** 插件已启用。
3. 重新生成工程文件并编译。
4. 在内容浏览器右键 → Miscellaneous → **Ability Asset** 创建技能资产，往 `Tasks` 里添加 Task。
5. 新建一个继承 `UAKGameplayAbility` 的蓝图，把 `AbilityAsset` 指向刚创建的资产，通过 ASC 授予并激活即可。

### 开发与验证环境

| 项 | 值 |
|---|---|
| 引擎 | Unreal Engine **5.8.3**（源码版，Branch `UE5`） |
| 依赖插件 | `GameplayAbilities`、`GameplayTags`（引擎自带 Runtime 插件） |
| 平台 | Win64 |

代码只使用跨版本稳定 API（`UGameplayAbility` / `UGameplayEffect` / `FGameplayTagContainer` / `FAssetTypeActions_Base` 等），
理论上可在其他 UE 5.x 上编译。若在某个版本上遇到问题，请提 Issue 并附上引擎版本。

编译验证：

```
<Engine>\Engine\Build\BatchFiles\RunUAT.bat BuildPlugin ^
  -Plugin="<Repo>\Plugins\UEAbilityKit\UEAbilityKit.uplugin" ^
  -Package="<Repo>\Build\Package" -TargetPlatforms=Win64
```

## 路线图

| 阶段 | 目标 |
|---|---|
| M0 | 仓库 + 插件骨架 + 最小资产模型（已完成） |
| M1 | 时间轴执行器 + GAS 桥接（GE / Cue / Tag 三类 Task、目标选择、预测与复制） |
| M2 | 编辑器可用：轨道拖拽、预览视口、Montage 长度对齐、单帧步进 |
| M3 | 治理能力：Validator、Json 导入导出（进 Git）、CI 校验 |
| M4 | 调试闭环：Runtime Debugger + Trace 导出 |
| M5 | 高级编排：ECA / Trigger 轨道、分支、复合轨道、碰撞与位移 Task |

详见 [docs/roadmap.md](docs/roadmap.md)。

## 致谢与合规声明

- 本项目的**架构思想**参考了开源项目 [AbilityKit](https://github.com/HOBOBO/AbilityKit)（MIT，Unity / 纯 C#）中关于技能管线、事件触发、溯源与配置治理的设计；**本项目为独立实现，未复用其任何代码**（C# 实现亦无法用于 UE）。
- 编辑器交互设计参考了商业插件 **Able** 的**公开可见的交互模式**（时间轴 UX、验证器、预览视口等通用做法）；**未复用其任何源码、资源与文档文案**，且本项目与其无隶属关系。
- `Unreal®` 是 Epic Games, Inc. 在美国及其他地区的商标或注册商标。本项目为社区独立项目，**非 Epic 官方产品，未获 Epic 认可**；本项目不使用 Epic 官方 Logo、字体与配色。

完整声明见 [NOTICE](NOTICE)。

## License

Apache License 2.0，见 [LICENSE](LICENSE)。
