# Content

示例工程的内容目录。

## AnimationAssets/

角色与动画，**来自 UnLuaNova 示例工程**（[HUSTJJD/UnLuaNova](https://github.com/HUSTJJD/UnLuaNova)，MIT；其中的 UE4 Mannequin 来自 Epic 示例内容）。

原样保留了目录路径 `/UEAbilityKit/AnimationAssets/...`，这样资产之间的内部引用（骨骼、材质、贴图）不会断。

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

> 这两个 Json 里引用的 `/UEAbilityKit/Demo/GE_*` 与 `GameplayCue.*` 需要你先创建，见下节。
> 缺资产时导入不会报错，对应任务只是拿不到 GE / Cue。

## Script/

`Content/Script/UAKDemoCharacter.lua` 是 UnLuaNova 绑定脚本（由
`Config/DefaultUnLuaNova.ini` 里的 `ClassModuleBindings` 把 `AKDemoCharacter`
映射到它 —— 改配置就能绑，不需要动 C++）。
脚本只负责项目特定的扩展：输入按下之后干什么、UI 联动、给引导条件喂"按键还按着吗"。
详见 [../docs/examples/04-scripting-with-unlua.md](../docs/examples/04-scripting-with-unlua.md)。

> 打包提醒：脚本不是资产，`Content/Script` 必须显式 stage（`DefaultGame.ini` 的
> `DirectoriesToAlwaysStageAsUFS`），插件自己的 Lua 运行时模块同理 —— 见 04 文档「打包」一节。

## 还需要在编辑器里创建的资产

**不用了。** 一条命令就能全部生成（技能资产 / 蓝图 / 关卡 / 默认地图）：

```powershell
UnrealEditor-Cmd.exe <你的工程>.uproject -run=UAKDemoSetup -unattended -nosplash
```

详见 [../docs/examples/05-run-the-demo.md](../docs/examples/05-run-the-demo.md)。

下面列出命令会生成什么，以及**不再需要**手工建的东西：

| 以前要手工建 | 现在 |
|---|---|
| `GE_MeleeDamage` / `GE_FireballDamage` / `GE_Cooldown_Sample` / `GE_Cost_Sample` | C++ 类 `Plugins/UEAbilityKit/Source/UAKDemo/UAKDemoEffects.h`（不进仓库二进制，冷却 Tag 用原生 Tag 注册） |
| `BP_SampleAbility` | 命令生成，CDO 自动填好 `AbilityAsset` / 冷却 / 消耗 |
| `BP_DemoCharacter` | 命令生成，自动挂 `SK_Mannequin` 与默认技能 |
| 关卡 + `GameDefaultMap` | `/UEAbilityKit/Demo/Maps/L_Demo`，并写进 `Config/DefaultEngine.ini` |

只有 **GameplayCue** 仍建议按需建（没有对应 GC 蓝图时 Cue 只是空转，不影响逻辑）：
在项目设置或 `DefaultGameplayTags.ini` 里加
`GameplayCue.Ability.Melee.Swing` / `.Impact` / `GameplayCue.Ability.Fireball.Channel`。

## 更多

- 工程结构与迁移记录：[../docs/examples/03-sample-project.md](../docs/examples/03-sample-project.md)
- 多人接入：[../docs/examples/01-minimal-multiplayer-setup.md](../docs/examples/01-minimal-multiplayer-setup.md)
- 冷却 / 消耗 / 堆叠：[../docs/examples/02-cooldown-cost-stacking.md](../docs/examples/02-cooldown-cost-stacking.md)
