# UAK Sample

UEAbilityKit 的**最小可编译示例插件**：一个带 ASC 的角色、一个属性集、一个带冷却与消耗的技能。

> 这是**源码示例**，不含二进制资产（角色网格、动画、GE、Cue 需要你在自己的项目里配）。
> 好处是不依赖任何 uasset，拷过去就能编译。

## 目录

```
Samples/UAKSample/
├─ UAKSample.uplugin
└─ Source/UAKSample/
   ├─ Public/UAKSampleAttributeSet.h     Health / MaxHealth / Mana / MaxMana（含复制与夹取）
   ├─ Public/UAKSampleCharacter.h        自带 ASC + 属性集，PossessedBy 时授予技能
   └─ Public/UAKSampleAbility.h          UAKGameplayAbility 的最小子类
```

## 使用步骤

1. 把 `Samples/UAKSample` 整个目录拷到你的项目 `Plugins/` 下（和 `UEAbilityKit` 同级）。
2. 启用 **UE Ability Kit** 与 **UAK Sample** 两个插件，重新生成工程并编译。
3. 在你的项目里创建：
   - `GE_Cooldown_Sample`：`Duration Policy = Has Duration`，时长 5s，`Granted Tags = Cooldown.Sample`
   - `GE_Cost_Sample`：`Duration Policy = Instant`，`Modifiers: Mana, Additive, -20`
   - `BP_SampleAbility`：继承 `UAKSampleAbility`，填 `AbilityAsset`（你的技能资产）、
     `SampleCooldownEffect`、`SampleCostEffect`
4. 创建 `BP_SampleCharacter` 继承 `AAKSampleCharacter`，把 `BP_SampleAbility` 填进 `Default Abilities`。
5. 用两个客户端 + Listen Server 跑（`-game` 加 `NumPlayers=2`），验证：
   - 技能在客户端先跑（预测），服务器校验
   - 冷却期间无法再次激活
   - 法力不足时无法激活
   - 伤害在两端一致

## 它示范了什么

| UAK 的设计原则 | 在示例里的体现 |
|---|---|
| 逻辑归 GAS | 属性、冷却、消耗、堆叠全是 GAS，示例里没有一行 UAK 网络代码 |
| 资产与技能解耦 | 同一条 `UAKAbilityAsset` 可被多个 `UAKSampleAbility` 以不同冷却引用 |
| 驱动交给 GAS | 执行器是 AbilityTask，技能结束自动收尾 |
| 表现交给 Cue | 示例不含粒子/音效，那部分归 `UGameplayCueNotify` |

## 注意

- ASC 挂在 Character 上是为了示例简单。**真实项目玩家请挂 PlayerState**
  （详见 `docs/examples/01-minimal-multiplayer-setup.md`）。
- 换角色/Possess 后需要 `RefreshAbilityActorInfo()`；示例只在 `PossessedBy` 初始化了一次。
