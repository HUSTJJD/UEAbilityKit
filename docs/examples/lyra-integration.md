# 把 Lyra 改造进本工程

## 三条原则

**1. 插件是产品，Lyra 只是示例宿主**

`Plugins/UEAbilityKit` 必须**完整且内聚**：它只认识 GAS，不认识任何具体游戏。
凡是必须提到 Lyra 的代码，一律不进插件，放进独立的适配层 `Source/LyraAdapt/`。

```
Plugins/UEAbilityKit/              ← 产品插件：零依赖，可服务任何游戏
   ├─ Source/UEAbilityKit/         ← 核心（时间轴、任务、GAS 桥接）
   ├─ Plugins/UEAbilityKit/Source/UAKDemo/              ← 示例模块（随插件走，不单独成工程）
   └─ Source/UEAbilityKitTests/    ← 测试
Source/LyraGame/                   ← 拷进来的 Lyra（保持 Lyra 命名，保留 Epic 版权头）
Source/LyraAdapt/                  ← 适配层：唯一同时认识 Lyra 与 UAK 的地方
```

`UAKDemo` **不是独立工程**，它是插件里的一个模块（`UAKDemo.uproject` 与 `UAKDemo*.Target.cs` 都已删除）。
任何项目装了这个插件就能用这套示例；本工程只是拿 Lyra 当宿主跑它。

**2. 继承方向是 Lyra 继承 UAK**

```
UGameplayAbility
   └─ UAKGameplayAbility          ← 插件提供
        └─ ULyraGameplayAbility   ← 拷进来的 Lyra，改一行继承
             └─ 各具体技能          ← 用 UAK 资产重写
```

**3. Lyra 有的文件，一律 copy → patch**

本工程就是 Lyra：`.uproject` **整个从 Lyra 拷来（连文件名一起是 `LyraStarterGame.uproject`）**，
`Config/*.ini`、`Source/*.Target.cs`、`Source/LyraGame/**` 同理——
**以 Lyra 的版本为基线**，再靠补丁把我们的东西加回去。
不是"合并"，不是"我们为主"，是 Lyra 打底、我们往上叠。

因此本工程不再有"自带的示例工程"：`UAKDemo` 降级成**插件的一个模块**，
随插件分发、任何项目都能用。我们替换的是 Lyra 的**技能实现**，不是重新做一个游戏。

---

## 两步走

### 1. 导入

```powershell
cd Tools
./import_lyra.ps1 -LyraPath D:\learning\LyraStarterGame     # 先去 Epic 下载 Lyra 5.8
```

```bash
cd Tools
./import_lyra.sh -l ~/learning/LyraStarterGame
```

| 步骤 | 内容 | 进 git？ |
| --- | --- | --- |
| 拷源码 | `Source/LyraGame`（20 个模块，完整可玩那套）+ `Source/LyraEditor` | ✅（约 2 MB） |
| 拷外围 | `.uproject`（**整个就是 Lyra 的，连名字**）、`Config/*.ini`、`Source/*.Target.cs`、`Platforms/` | ✅（原文件留 `.uak.bak`） |
| 拷资产 | `Plugins/GameFeatures/*`（约 1.4 GB） | ❌（已加 `.gitignore`） |
| 打补丁 | 见下 | ✅ |

排除没拷的 Lyra 模块：`Settings`、`Hotfix`、`Performance`、`Replays`、`Development`、`Tests`。

### 2. 打补丁

```powershell
python Tools/patch_lyra.py           # 打
python Tools/patch_lyra.py --check   # 只看状态
python Tools/patch_lyra.py --revert  # 撤销（按 .orig 还原）
```

补丁全部声明在 **`Tools/lyra_patches.json`** 里，那个文件就是适配层的完整定义：

```json
{
  "source": [
    { "id": "ability-base", "file": "Source/LyraGame/.../LyraGameplayAbility.h",
      "replace": [{ "from": "public UGameplayAbility", "to": "public UAKGameplayAbility" }],
      "insert_include": "UAKGameplayAbility.h" },
    { "id": "build-dep", "file": "Source/LyraGame/LyraGame.Build.cs",
      "replace": [{ "from": "\"GameplayTags\",", "to": "\"GameplayTags\",\n\t\t\t\"UEAbilityKit\"," }] }
  ],
  "uproject": { "modules": ["UAKDemo", "LyraAdapt"], "plugins": ["UEAbilityKit"] },
  "targets": { "add_modules": ["UAKDemo"] },
  "config":  { "append": { "DefaultGameplayTags.ini": "Tools/patches/config/DefaultGameplayTags.ini.append" } }
}
```

幂等：已打过的自动跳过；每处改动先备份 `<file>.orig`。

### 3. 编译

```powershell
Scripts\build.ps1
```

---

## 适配层放什么（`Source/LyraAdapt/`）

判断标准只有一条：**这段代码必须提到 Lyra 吗？**
是 → 放适配层；否 → 应该在插件里（或本来就是通用的）。

| 例子 | 归属 | 理由 |
| --- | --- | --- |
| 时间轴驱动、ApplyGE、Cue、命中查询 | **插件** | 只依赖 GAS，任何游戏通用 |
| `ULyraAdaptInputCondition`（按住维持） | 适配层 | 要读 Lyra 的 Enhanced Input |
| 调 Lyra `ULyraRangedWeaponInstance` 的开火任务 | 适配层 | 提到 Lyra 的武器类 |
| 设置 Lyra CameraMode 的任务 | 适配层 | 提到 `ULyraHeroComponent` |
| 生成"引用 Lyra 资产的技能"的 commandlet | 适配层 | 路径里全是 Lyra 的东西 |

现在里面已经有：`ULyraAdaptInputCondition`——Lyra 的 Pressed/Released 调 `SetHeld()`，
挂到 UAK 资产的 Channeling 条件上就是"松手打断吟唱"。

---

## 技能重写（我们的核心：换掉 Lyra 的整套技能体系）

| Lyra 原技能 | UAK 资产 | 复用 Lyra 的什么 |
| --- | --- | --- |
| 射击 | Montage → 开火任务（走武器实例）→ ApplyGE(SetByCaller) → Cue | Montage、伤害 GE、Cue、武器数据资产 |
| 换弹 | Montage + Channeling（移动打断）+ 补弹 GE | 弹药属性 / GE |
| 冲刺 | Dash + LooseTags + Cue | `GameplayCue.Character.Dash` |
| 近战 | Cue + 伤害 GE | `GameplayCue.Weapon.Melee.*` |

授予回到 Lyra 正路：`ULyraAbilitySet` / `ULyraPawnData`（代码在我们手里了）。
InputTag 沿用 Lyra 原生那批：

```
InputTag.Ability.Melee / .Dash / .Heal
InputTag.Weapon.Fire / .FireAuto / .Reload / .Grenade / .ADS
```

---

## 为什么 `UAKGameplayAbility` 不持有 Runner

早期为了让"UAK 去适配 Lyra"做过一个 `UUAKAbilityRunner`（Lyra 的能力类持有它）。
那是个方向错误：**变成插件在配合宿主**。

现在靠继承就够：Lyra 的能力类本来就 `Super::ActivateAbility(...)` / `Super::EndAbility(...)`
往上调，继承链里插进 UAK 之后时间轴自动被驱动，Lyra 自己的
ActivationPolicy / ActivationGroup / AdditionalCosts / SetCameraMode 全部保留。
Runner 已删除。

关键是 `UAKGameplayAbility` 在 **`AbilityAsset` 为空时什么都不做**——
Lyra 的跳跃、被动这些纯 GAS 技能没有时间轴可跑，不会被基类误杀。

---

## 许可与归属（务必保留）

- 拷进来的 Lyra 代码**保持 Lyra 命名**，文件头 Epic 版权声明一个字都别删
- 来源与改动记录在 `Tools/lyra_manifest.txt` + `THIRD_PARTY/Lyra-origin.md`
- **Lyra 代码只放 `Source/` / `Config/`，绝不进 `Plugins/UEAbilityKit`**

## 常见问题

| 现象 | 原因 |
| --- | --- |
| 编译报找不到 `UEAbilityKit` | `patch_lyra.py` 没跑，或 `LyraGame.Build.cs` 被 `-Force` 覆盖回去了 |
| 所有 Lyra 技能一激活就被取消 | UAK 的"资产为空即 no-op"逻辑没生效，重跑补丁并确认代码 |
| uproject 里我们自己的模块没了 | 补丁只做"补充"不做"删除"；用 `--check` 看，再 `--revert` 后重来 |
| 资产没进工程 | 跑了 `-SkipAssets`；重跑一次不带这个参数 |
