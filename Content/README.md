# Content

示例工程的内容目录。

## AnimationAssets/

角色与动画，**来自 UnLuaNova 示例工程**（[HUSTJJD/UnLuaNova](https://github.com/HUSTJJD/UnLuaNova)，MIT；其中的 UE4 Mannequin 来自 Epic 示例内容）。

原样保留了目录路径 `/Game/AnimationAssets/...`，这样资产之间的内部引用（骨骼、材质、贴图）不会断。

```
AnimationAssets/
├─ UE4_Mannequin/
│  ├─ Mesh/SK_Mannequin.uasset           ← 角色网格
│  ├─ Mesh/UE4_Mannequin_Skeleton.uasset ← 骨骼
│  ├─ Mesh/SK_Mannequin_PhysicsAsset.uasset
│  ├─ Materials/…
│  └─ Textures/…
├─ Idle_Rifle_Hip.uasset      ← 待机
├─ Jog_Fwd_Rifle.uasset       ← 前进（另有 Bwd / Lt / Rt）
├─ Enemy_Idle / Enemy_Walk.uasset
├─ Death_1 / Death_2 / Death_3.uasset
└─ AM_Walk.uasset             ← AnimMontage（演示 PlayMontage 任务用）
```

用法：`BP_DemoCharacter`（继承 `AAKDemoCharacter`）的 Mesh 指定 `SK_Mannequin` +
`UE4_Mannequin_Skeleton` 即可直接用。

## Demo/Abilities/

**技能资产示例，以 Json 形式提供** —— 这是 UAK 的核心主张之一：技能是纯数据，
能进 Git、能 diff、能被 CI 校验，不是二进制黑盒。

| 文件 | 演示点 |
|---|---|
| `AB_MeleeSwing.json` | 近战三段式：Montage + GE 伤害 + GameplayCue（逻辑与表现分离，两条轨道） |
| `AB_ChanneledFireball.json` | 引导（移动超过阈值就打断）+ Sequence 编排 + 起始延迟 |

导入：编辑器里打开一个 `Ability Asset`，用 UAK 编辑器的 Json 导入功能载入这些文件
（或直接改好后用「导出」回写，格式与 `UAKAbilityJsonSerializer` 完全一致）。

> 这两个 Json 里引用的 `/Game/Demo/GE_*` 与 `GameplayCue.*` 需要你先创建，见下节。
> 缺资产时导入不会报错，对应任务只是拿不到 GE / Cue。

## Script/

`Content/Script/UAKDemoCharacter.lua` 是 UnLuaNova 绑定脚本（由 `AAKDemoCharacter::GetModuleName` 指定）。
脚本只负责项目特定的扩展：输入绑定、UI 联动、自定义引导条件。
详见 [../docs/examples/04-scripting-with-unlua.md](../docs/examples/04-scripting-with-unlua.md)。

## 还需要在编辑器里创建的资产

Json 示例之外，下面这些必须手动建（都是 GAS 原生物件，无法用文本表达）：

1. **Content/Demo/** 新建 GameplayEffect：
   - `GE_MeleeDamage`：Instant，Modifiers: Health Additive -30（用 SetByCaller `Damage` 覆盖更好）
   - `GE_FireballDamage`：Instant，Modifiers: Health Additive -25
   - `GE_Cooldown_Sample`：Has Duration 5s，Granted Tags = `Cooldown.Sample`
   - `GE_Cost_Sample`：Instant，Modifiers: Mana Additive -20
2. **GameplayCue**：在 `DefaultGameplayTags.ini` 或项目设置里加
   `GameplayCue.Ability.Melee.Swing` / `.Impact` / `GameplayCue.Ability.Fireball.Channel`，
   再按需建 `GC_` 蓝图（没有对应 GC 时 Cue 只是空转，不影响逻辑）
3. **Content/Abilities/** 新建 `BP_SampleAbility` 继承 `UAKDemoAbility`，
   填 `AbilityAsset` / `SampleCooldownEffect` / `SampleCostEffect`
4. 新建 `BP_DemoCharacter` 继承 `AAKDemoCharacter`，Mesh 用 `SK_Mannequin`，
   把 `BP_SampleAbility` 填进 `Default Abilities`
5. 新建一个关卡，把 `BP_DemoCharacter` 放进去并设为 `GameDefaultMap`

## 更多

- 工程结构与迁移记录：[../docs/examples/03-sample-project.md](../docs/examples/03-sample-project.md)
- 多人接入：[../docs/examples/01-minimal-multiplayer-setup.md](../docs/examples/01-minimal-multiplayer-setup.md)
- 冷却 / 消耗 / 堆叠：[../docs/examples/02-cooldown-cost-stacking.md](../docs/examples/02-cooldown-cost-stacking.md)
