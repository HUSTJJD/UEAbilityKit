# 路线图

## M0 — 仓库与插件骨架 ✅

- [x] 仓库治理：`README` / `NOTICE` / `LICENSE`(Apache-2.0) / `.gitignore` / `.gitattributes`
- [x] 插件骨架：`UEAbilityKit.uplugin` + Runtime 模块 + Editor 模块
- [x] 资产模型：`UAKAbilityAsset`（时间轴数据）
- [x] Task 基类：`UAKAbilityTaskBase`
- [x] GAS 桥接：`UAKGameplayAbility`（M0 为瞬时执行）
- [x] 示例 Task：`UAKAbilityTask_ApplyGameplayEffect`
- [x] 内容浏览器可创建 Ability Asset

## M1 — 时间轴执行器 + GAS 桥接（进行中）

- [x] **两级数据模型**：`UAKAbilityAsset` → `UAKAbilityTrack`（轨道容器）→ `UAKAbilityTaskBase`（片段）
- [x] **资产与 Ability 解耦（N:M）**：资产只描述时间轴，冷却/消耗/目标/网络策略归 `UAKGameplayAbility`
- [x] **Task 三契约**：生命周期 / `Validate()` 自校验 / `GetGASImpact()` 自述 GAS 影响
- [x] **`UAKAbilityInstance`**：按时间轴推进，支持 Duration / bLoop / MaxLoops / 中断 / PlayRate / ScratchPad
- [x] **驱动方式定稿**：`UAKAbilityTickTask`（`UAbilityTask` 子类，`bTickingTask=true`）——
      技能结束 → 任务自动结束 → 时间轴自动收尾；网络预测与复制仍由 GAS 负责
- [x] **执行端过滤**：`EAKTaskNetMode`（Both / ServerOnly / ClientOnly），避免在错误的一端空跑
- [x] 内置 Task：`PlayMontage`、`SendGameplayCue`、`CollisionQuery`、`Dash`、`ApplyGameplayEffect`
- [x] 目标选择接入 `FGameplayAbilityTargetDataHandle`（`UAKTargeting`，只做适配不自建目标选择系统）
- [ ] 冷却 / 消耗 / 堆叠示例（GAS 原生能力，UAK 不自建，只需示例展示如何配 → 见「剩余工作」）
- [ ] **编译验证**（当前被另一实例的 UBT 互斥锁阻塞 → 见「剩余工作」）

## M2 — 编辑器 ✅

- [x] `FUEAbilityKitAssetEditor`（`FAssetEditorToolkit`）：Timeline / Inspector / Bloodline 三 Tab
- [x] `SUAKTimeline` 自绘时间轴：标尺、网格、轨道头（点击整轨静音）、片段、Markers、光标
- [x] 通用交互：拖拽移动片段、滚轮缩放、按帧吸附、单帧步进、帧/秒切换
- [x] `SUAKGASImpactBar`：随光标实时汇总 GE / Cue / Tags（**UAK 独创**）
- [x] `SUAKValidationBar`：常驻底栏，改动即增量重算（Able 是弹窗）
- [x] `SUAKBloodlineView`：血缘 / Explain 视图（设计期用 `GetGASImpact`，运行时接 `UAKTrace`）
- [x] `UAKAbilityAssetFactory`：内容浏览器 Right-click → Miscellaneous → Ability Asset
- [x] 预览 Tab（`FPreviewScene` + 预览角色 + Montage 播放 / 停止 / 循环）
- [x] Task Picker（搜索 + 列表 + 指定目标轨道 + 支持蓝图派生 Task）
- [x] `UAKAbilityThumbnailRenderer`：内容浏览器里画"技能结构示意图"
- [x] 工具条：播放/暂停/回到起点/单帧步进、按帧显示、资产属性、+标记、从 Montage 生成
- [x] 轨道管理：增删 + 上下移动（影响执行顺序与绘制行序）
- [x] 任务管理：复制（Ctrl+D）/ 删除（Delete）
- [x] 快捷键：空格播放暂停、←→ 单帧（Ctrl 十帧）、Ctrl+D 复制、Delete 删除、滚轮缩放
- [x] 从 Montage 生成：按长度设置技能长度 + 全长 PlayMontage 任务 + **Notify 转 Marker**
- [ ] 撤销/重做的事务包装与 Undo 菜单集成
- [ ] 任务的 Alt+拖拽复制与右键菜单

## M3 — 治理能力 ✅

- [x] Task 自带 `Validate()`（内聚，而非外部绑定 Validator）+ 资产级规则
- [x] `UAKAbilityJsonSerializer`：资产 ↔ Json 双向（`$type` 定位类、`version` 迁移、Preview 字段不导出）
- [x] 校验 UI 与指纹增量重算
- [x] `UAKValidateCommandlet`：`-run=UAKValidate -path=... -strict -json`，可在 CI 中作为门禁

## M4 — 调试闭环 ✅（数据结构与视图，实况联调待做）

- [x] `FAKTraceNode` / `UAKTrace`：血缘树（ParentIndex 构树，可下钻）
- [x] `UAKTraceSubsystem`：世界级记录器，可开关（关闭时零开销）
- [x] 血缘视图支持运行时实况数据源（`SetRuntimeTrace`）
- [x] 执行器把每次施放写入 Trace（`UAKAbilityInstance::SetTrace`，任务进入时建节点）
- [x] PIE 联动：光标跟随最晚节点时间 + 血缘视图自动切实况 + PIE 期间自动开启记录

## M5 — 高级编排（部分）

- [x] Rule 轨道：`UAKAbilityTask_Branch`（Tag / 属性条件 → 开关另一条轨道）
- [x] 执行器支持 `Context.DisabledTracks`（分支结果过滤）
- [x] 复合轨道：`Sequence` / `Parallel` / `RepeatCount` / `RepeatInterval` / `StartDelay`
- [x] `BuildExecutionPlan()`：复合模式展开成扁平调度表，编辑器与运行时共用
- [x] `UAKAbilityTask_WaitUntil`：条件满足即继续 + `bGateTimeline` 冻结时间轴
- [x] `FAKCondition`：Branch 与 WaitUntil 共用同一套条件语义
- [x] `UAKQuery`：`CollisionQuery` / `AreaEffect` / 投射物共用同一份查询与过滤规则
- [x] `AAKProjectile`：服务端权威，逐帧扫掠推进，命中打 GE + Cue，支持穿透/重力/超时
- [x] `UAKAbilityTask_SpawnProjectile`：插槽/偏移生成、瞄准首个目标、`SpawnCount`+`SpreadAngle` 扇形弹幕
- [x] `UAKAbilityTask_AreaEffect`：周期生效、可跟随/固定中心、每目标只吃一次（ScratchPad 演示）
- [x] 打断传播：`EAKInterruptOrder`（后启动先结束 / 顺序 / 轨道优先级）
- [x] 轨道被 Branch 停用 → 正在运行的任务按"取消"结束（与技能中断区分）
- [x] `bAlwaysExecute`：即使被中断也保证执行一次（未轮到的补执行）
- [x] 门控任务在中断时必然解除（清空活跃列表，避免时间轴永久冻结）
- [x] **引导 / 吟唱（Channeling）**：窗口 + 维持条件 + 三种失败动作
      （Tag/属性、移动速度、距离、自定义；`bEvaluateChannelingOnAuthorityOnly`）
- [x] **TaskId**：同类任务的稳定标识符，Trace 节点优先用它命名
- [x] **Task 编辑器元数据**：集中式元数据表（分类 / 显示名 / 说明 / 配色 / 废弃标记），
      Task Picker 与时间轴都已使用
- [x] **查询**：新增 Cone（锥形）形状；过滤器与形状解耦（`Tag` / `Distance` / `LineOfSight`）
- [x] **撤销 / 重做**：所有改资产的操作包进 `FScopedTransaction`；
      编辑器实现 `FEditorUndoClient`（`PostUndo` / `PostRedo`），Ctrl+Z / Ctrl+Y 生效
- [x] **Alt+拖拽复制**：拖动时按住 Alt 先复制再拖（复制返回新任务供拖拽接管）
- [x] **右键菜单**：任务（复制/删除）、轨道（增删/上下移）、资产（加标记/属性/从 Montage 生成）
- [x] **引导条件 Json 序列化**：`ChannelingConditions` 按 `$type` 进出 Json
- [x] **血缘下钻到 GE / 目标**：`Context.Trace` + `CurrentTaskNodeIndex`，
      ApplyGE 记 GE→SetByCaller→目标，CollisionQuery 记命中目标
- [x] 多人接入示例：[examples/01-minimal-multiplayer-setup.md](examples/01-minimal-multiplayer-setup.md)
- [x] 冷却 / 消耗 / 堆叠示例：[examples/02-cooldown-cost-stacking.md](examples/02-cooldown-cost-stacking.md)
- [x] 多人示例：`Source/UAKDemo/` —— 工程自带的示例模块
      （ASC + 属性集 + 带冷却/消耗的技能）
- [x] **脚本方案**：`Plugins/UnLuaNova` 以 git submodule 引入，仅示例模块依赖；
      UAK 插件本身不依赖任何脚本系统（核心 C++，扩展用 Lua）
- [x] **工程化改造**：仓库本身成为可打开的 UE 工程（对照 UnLua 布局），
      UAK 作为 `Plugins/UEAbilityKit`；示例插件用 `git mv` 迁为工程模块 `Source/UAKDemo`

## 剩余工作

> 说明：上面另有一个 "M5 — 高级编排（部分）" 小节，本节只汇总**尚未完成**的事项，
> 避免两处清单重复导致状态不一致。

| 事项 | 状态 | 说明 |
|---|---|---|
| **编译验证** | ❌ 阻塞中 | 唯一的功能性阻塞：另一实例的 UBT 互斥锁 |
