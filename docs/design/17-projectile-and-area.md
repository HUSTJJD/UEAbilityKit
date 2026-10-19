# 投射物与区域效果设计（已定稿，M5）

## 1. 总原则：落到 GAS，不自建物理与持续效果运行时

AbilityKit 有独立的 `combat.projectile` 模块和 `continuous` 运行时。
在 UE 里这两块都有更好的归属：

| 能力 | UAK 的做法 |
|---|---|
| 移动与命中检测 | 引擎的扫掠/重叠查询（`UAKQuery`），不自建物理 |
| 命中后的伤害 / Buff / 治疗 | `UGameplayEffect` |
| 命中表现 | `UGameplayCueNotify` |
| 持续 / 周期 / 打断 / 移除 | GE 的 Duration / Period / Tag Requirements |
| 网络 | 服务端权威生成 + ASC 复制 + Cue 复制 |

**因此 UAK 的"投射物"只负责三件事：飞、撞到谁、把 GE 和 Cue 打上去。**

## 2. `AAKProjectile`

```
Spawn（服务端） → 每帧 SweepTo 推进 → 命中 → ApplyHit(GE + Cue) → 销毁 / 穿透
```

| 属性 | 说明 |
|---|---|
| `Radius` / `TraceChannel` | 扫掠球半径与通道 |
| `Speed` / `GravityScale` | 初速与重力（0 = 直线） |
| `MaxLifetime` | 超时销毁，兜底 |
| `bDestroyOnHit` / `MaxPierceCount` | 命中即销毁 / 可穿透几个 |
| `FlightCueTag` | 飞行中的持续表现（`AddCue` / 销毁时 `RemoveCue`） |

- `bReplicates = true` + `SetReplicateMovement(true)`：表现可见；
- 逻辑（施加 GE）只在生成端（服务端）发生，与 `NetMode = ServerOnly` 配套。

## 3. `UAKAbilityTask_SpawnProjectile`

- 默认 `ServerOnly`
- 生成点：骨骼插槽（`SocketName`）或相对偏移（`SpawnOffset`）
- 方向：`bAimAtFirstTarget` 朝第一个目标，否则施法者正前方
- **弹幕**：`SpawnCount` + `SpreadAngle` 做水平扇形散布
- 命中效果：`HitEffectClass`（GE）+ `HitCueTag`

## 4. `UAKAbilityTask_AreaEffect`

周期性区域效果，`Duration` 决定持续多久，`Interval` 决定多久生效一次。

- 默认 `ServerOnly`、`bNeedsTick = true`
- `bFollowOwner`：区域中心是否跟随施法者移动；否则固定首次生效时的位置
- `bApplyOncePerTarget`：每个目标只吃一次（否则每个 Interval 都会吃到）
- `MaxTargetsPerTick`：每次生效的目标上限

**为什么不用 GE 的 Period 直接做？**
GE 的周期只作用于"同一批目标"（施加时的目标）。区域效果的目标集合会随时间变化（有人走进/走出），
所以需要 UAK 这一层周期性地重新查询并施加——这是 GE 做不到、必须自己做的部分。

## 5. ScratchPad 演示

`AreaEffect` 的周期计时与已命中列表存在 `UAKAreaEffectScratchPad` 里，
通过 `CreateScratchPad()` 外置——**任务对象本身无状态**，因此同一份资产可被多个实体并行运行。

## 6. `UAKQuery`：三处共用

`CollisionQuery` 任务、`AreaEffect` 任务、`AAKProjectile` 全部调用同一个 `UAKQuery`，
保证过滤规则（忽略施法者、要求 ASC、结果上限）在三个地方完全一致。

## 7. 已知限制

- 投射物不做预测：客户端不会本地预生成（避免双重生成）。若需要手感，建议用 Cue 做客户端即时表现。
- 投射物的碰撞只处理"第一个/前 N 个命中"，不做弹射与反弹。
- 区域效果目前只有球形；盒/胶囊区域可复用 `UAKQuery`（尚未在任务上暴露）。
