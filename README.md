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

> 待完成：PIE 联动（时间轴光标跟随 + 血缘视图切实况），以及**编译验证**。

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
