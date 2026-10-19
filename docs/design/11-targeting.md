# 目标选择设计（已定稿，M1）

## 1. 原则：不自建目标选择系统

GAS 已有完整机制（`FGameplayAbilityTargetDataHandle`、`UAbilityTask_WaitTargetData`、TargetActor、
服务器确认流程、客户端预测与复制）。UAK 只做**适配**：把 TargetData 取出来放进上下文。

```
GAS 产生 TargetData
   ↓ UAKTargeting::AppendTargetDataToContext
FAKAbilityContext.Targets
   ↓ 后续效果任务消费
ApplyGameplayEffect / SendGameplayCue / ...
```

## 2. 两个来源

| 来源 | 时机 | 实现 |
|---|---|---|
| **事件触发** | `ActivateAbility` 时 | `TriggerEventData->TargetData` → 直接写入 Context |
| **时间轴内查询**（`CollisionQuery` 任务） | 时间轴推进到该任务时 | 世界重叠查询 → 写入 Context |

两者可以叠加：`CollisionQuery` 的 `bAppendToTargets` 决定是追加还是替换。

## 3. API（在 5.8.3 源码核对）

```cpp
FGameplayAbilityTargetDataHandle::Data          // TArray<TSharedPtr<FGameplayAbilityTargetData>>
FGameplayAbilityTargetData::GetActors()         // TArray<TWeakObjectPtr<AActor>>
```

## 4. 契约

- 目标集合是**运行数据**，属于 `FAKAbilityContext`，不写回资产。
- 目标以 `TWeakObjectPtr<AActor>` 保存，避免阻止 GC。
- `CollisionQuery` 默认 `bRequireAbilitySystem = true`，过滤掉没有 ASC 的 Actor（桌椅板凳）。
- 顺序保证：查询顺序即写入顺序；`MaxTargets` 可截断。

## 5. 形状与过滤器分离（M5）

学自 AbleSP 的 Targeting 设计：**形状回答"在哪查"，过滤器回答"哪些算数"**，两者可自由组合。

| 形状 | 说明 |
|---|---|
| `Sphere` / `Box` / `Capsule` | 基本形状 |
| `Cone` | 锥形（近战最常见）：以轴向为中心、`ConeAngle` 为半角；
  实现上用球做广相，命中后按夹角筛（窄相） |

| 过滤器 | 说明 |
|---|---|
| `Tag` | 目标 ASC 需要 / 不能拥有指定 Tag |
| `Distance` | 距离阈值，可选只算水平、计入胶囊半径 |
| `LineOfSight` | 射线检测，被遮挡即排除 |

组合示例：`Cone(60°) + Tag(需 State.Burning) + Distance(8m) + LineOfSight`

过滤器是 `UAKQueryFilter` 的 `Instanced` 数组，`CollisionQuery` 与 `AreaEffect` 都能挂。

## 6. 待补（M5+）

- 从 `FGameplayAbilityTargetDataHandle` 读取 `HitResult` 的位置信息，供 Cue 定位。
- 由项目注入自定义 Targeting（例如 "最近的 N 个敌人"）：建议做成自定义 Task，而不是改框架。
- 过滤器可扩展：目前三个内置，后续按项目需要加（阵营、高度差、朝向）。
