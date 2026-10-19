# 最小多人接入示例

> 目标：让一条 UAK 技能在 Listen Server + 1 Client 下正确预测与复制。
> **UAK 不提供任何网络代码**——网络正确性来自"所有副作用都落在 GAS 原语上"。

## 1. ASC 放哪

| 角色 | 建议 |
|---|---|
| 玩家 | ASC 放 **PlayerState**（死亡重生后属性/技能保留） |
| AI / 小怪 | ASC 放 **Character**（随角色销毁） |

```cpp
// 玩家 Character：实现 IAbilitySystemInterface 并转发到 PlayerState 的 ASC
UAbilitySystemComponent* AMyCharacter::GetAbilitySystemComponent() const
{
	return PlayerState ? PlayerState->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
}
```

两者都要在 `BeginPlay` 里 `InitAbilityActorInfo(OwnerActor, AvatarActor)`，
并在 Pawn 被 Possess / 换角色后调用 `ASC->RefreshAbilityActorInfo()`。

## 2. 复制模式

```cpp
ASC->SetReplicationMode(EGameplayEffectReplicationMode::Mixed); // 玩家
ASC->SetReplicationMode(EGameplayEffectReplicationMode::Minimal); // AI
```

## 3. 技能配置（`UAKGameplayAbility` 蓝图/CDO）

| GAS 字段 | 值 | 说明 |
|---|---|---|
| `Instancing Policy` | `Instanced Per Actor` | UAK 执行器需要实例 |
| `Net Execution Policy` | `Local Predicted` | 客户端先跑，服务器校验 |
| `Net Security Policy` | `Client Or Server` | — |
| `AbilityAsset` | 指向一条 `UAKAbilityAsset` | UAK 唯一需要的字段 |

## 4. 授予与激活

```cpp
ASC->GiveAbility(FGameplayAbilitySpec(MyAKAbilityClass, 1, INDEX_NONE, this));
ASC->TryActivateAbilityByClass(MyAKAbilityClass);
```

## 5. 任务的执行端要配对

| 任务 | 建议 `NetMode` |
|---|---|
| `ApplyGameplayEffect`（伤害/Buff） | `Both`（或 `ServerOnly` 若只要权威结算） |
| `CollisionQuery` | `ServerOnly`（目标以服务器为准） |
| `PlayMontage` / `SendGameplayCue` | `ClientOnly`（表现不占服务器） |
| `SpawnProjectile` | `ServerOnly`（已在默认值里设好） |
| `AreaEffect` | `ServerOnly`（已在默认值里设好） |

## 6. 常见坑

| 现象 | 原因 |
|---|---|
| 客户端看到伤害又回滚 | GE 没走预测：确认 `Net Execution Policy = Local Predicted`，且 GE 的 `Duration Policy` 与属性复制设置正确 |
| 蒙太奇不复制 | 直接调了 `AnimInstance->Montage_Play`；要用 UAK 的 `PlayMontage` 任务（它走 `ASC->PlayMontage`） |
| 特效只在一端出现 | Cue 没走 `UGameplayCueNotify`；不要直接在任务里 Spawn 粒子 |
| 目标不一致 | 查询任务设成了 `Both`；改成 `ServerOnly`，并由服务器下发 TargetData |
