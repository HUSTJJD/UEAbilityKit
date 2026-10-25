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

### 0. 先清，再导（每次都从干净状态开始）

拷进来的 Lyra **一个字都不许手改**：改过的 Lyra 会让编译/测试"因为改过而通过"，把 bug 藏起来。
必须改的地方全部写在 `Tools/lyra/lyra_patches.json` 里，由 `lyra/patch.py` 自动打。
所以导入脚本默认**先清一遍再拷**，保证工程永远等于：

```
Lyra 原版  +  Tools/lyra/lyra_patches.json 里声明过的补丁      （别的什么都没有）
```

```powershell
cd Tools
./lyra.ps1 clean                       # 一键清除 Lyra（源码/外围/资产/补丁痕迹）
./lyra.ps1 clean --keep-assets           # 只清源码与配置，1.4 GB 资产留着（迭代快）
./lyra.ps1 clean --deep                 # 连 Binaries/Intermediate/Saved 一起清
./lyra.ps1 clean --dry-run               # 只列出来，不真删
```

```bash
./lyra.sh clean            # 同上，Linux / macOS
./lyra.sh clean --deep
```

一键重来（= 清 + 拷 + 打补丁 + 校验，一条命令复现）：

```powershell
./lyra.ps1 import --lyra-path <你的 Lyra 目录>
./lyra.ps1 import --lyra-path <你的 Lyra 目录> --keep-assets   # 资产沿用上次的
```

```bash
./lyra.sh import -l <你的 Lyra 目录>
```

`lyra import` 会顺手记两样东西：

| 文件 | 用途 |
| --- | --- |
| `Tools/lyra/.lyra_paths.txt` | 这次拷了哪些路径，`lyra clean` 照单全删 |
| `Tools/lyra/.lyra_state.json` | 每个文件的 sha256 快照（Lyra 原版 / 打完补丁后各一份） |

### 1. 导入（内部已经包含第 0 步的清理）

```powershell
./lyra.ps1 import --lyra-path <你的 Lyra 目录>     # 先去 Epic 下载 Lyra 5.8
```

| 步骤 | 内容 | 进 git？ |
| --- | --- | --- |
| 预清理 | `lyra clean`：删掉上一次导入的残留与手改 | — |
| 拷源码 | `Source/LyraGame`（20 个模块，完整可玩那套）+ `Source/LyraEditor` | ❌ 不进（约 2 MB，拷来的，`lyra clean` + `lyra import` 随时复现） |
| 拷外围 | `.uproject`（**整个就是 Lyra 的，连名字**）、`Config/*.ini`、`Source/*.Target.cs`、`Platforms/` | ❌ 不进（`Config/` 已加 `.gitignore`） |
| 拷资产 | `Plugins/GameFeatures/*`（约 1.4 GB） | ❌（已加 `.gitignore`） |
| 打补丁 | 见下（含把我们的配置**追加**进 Lyra 的 ini） | ✅（补丁声明 `Tools/lyra/lyra_patches.json` 进仓库，被打进文件的内容不进） |
| 记账 + 校验 | `.lyra_paths.txt`、`.lyra_state.json` | ❌（运行期状态） |

排除没拷的 Lyra 模块：`Settings`、`Hotfix`、`Performance`、`Replays`、`Development`、`Tests`。

### 1b. 校验（有没有人手改过 Lyra）

```powershell
./lyra.ps1 verify -l <你的 Lyra 目录>   # 与 Lyra 源逐文件比 sha256（权威）
./lyra.ps1 verify                       # 离线：只跟导入时的快照比（快）
./lyra.ps1 verify --self-test           # 脚本自检，不碰工程
```

```bash
./lyra.sh verify -l <你的 Lyra 目录>    # 同上，Linux / macOS
```

> 想直接调 Python（比如在别的脚本里）：`python Tools/lyra/cli.py verify ...`。
> shell 入口只是把参数原样转发过去。

判定规则：

| 情况 | 结论 |
| --- | --- |
| 与 Lyra 原版字节一致 | ✅ |
| 不一致，但 `lyra_patches.json` 声明过、且补丁内容确实生效 | ✅ 已声明的补丁 |
| 不一致且没声明 | ❌ 手改 —— 清掉重来 |
| 打完补丁后又改了一行 | ❌ 手改（靠 `.lyra_state.json` 的快照抓出来） |
| 声明了却没打上 | ❌ 补丁缺失 |

退出码 `0` 干净 / `1` 有问题 / `2` 用法错误。**测不过就别提交，先 `lyra clean` 再 `lyra import`。**

### 2. 打补丁

```powershell
./lyra.ps1 patch           # 打
./lyra.ps1 patch --check   # 只看状态
./lyra.ps1 patch --revert  # 撤销（按 .orig 还原）
```

补丁全部声明在 **`Tools/lyra/lyra_patches.json`** 里，那个文件就是适配层的完整定义：

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
  "config":  { "append": { "DefaultGameplayTags.ini": "Tools/lyra/patches/config/DefaultGameplayTags.ini.append" } }
}
```

幂等：已打过的自动跳过；每处改动先备份 `<file>.orig`。

**机器相关的改动**单独放 `Tools/lyra/lyra_patches.local.json`（不进仓库，已加 `.gitignore`），
它会合并到上面那份基线之上。典型用途：本机用的是**源码引擎**，需要把 `EngineAssociation`
从 `"5.8"` 换成这台机器注册表里的 GUID——**仓库里不许出现任何本机路径或 GUID**，
所以这个值由导入脚本自己查出来写进去：

```powershell
./lyra.ps1 import --lyra-path <你的 Lyra 目录> --engine-root <你的引擎目录>
# → 自动查 HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds，把这个引擎的 GUID 写进
#   Tools/lyra/lyra_patches.local.json；查不到就设环境变量 UE_ENGINE_GUID
```

```json
{ "uproject": { "set_engine_association": "{本机引擎的 GUID}" } }
```

基线归基线、本机归本机：换台机器不会互相污染，`lyra clean` → `lyra import` 之后两边都能复现。

### 3. 编译

```powershell
Tools\build.ps1
```

---

## 配置（`Config/`）怎么管

**`Config/` 整个不进 git。** 我们那三个 ini（`DefaultEngine` / `DefaultGame` / `DefaultInput`）
Lyra 全都有，按 copy → patch 原则：Lyra 的拷进去，我们真正需要的那几行**追加**在末尾。

| 我们原本的 | 归属 |
|---|---|
| `GameDefaultMap` | 由 commandlet 写（且只在显式 `-SetDefaultMap` 时），不在这里维护 |
| `ProjectID` / `ProjectName` | Lyra 的（宿主就是 Lyra） |
| 旧式 `+ActionMappings`（UAK_Ability1/2） | **我们的** → `Tools/lyra/patches/config/DefaultInput.ini.append` |
| `+DirectoriesToAlwaysStageAsUFS`（UnLuaNova 脚本） | **我们的** → `Tools/lyra/patches/config/DefaultGame.ini.append` |
| `DefaultUnLuaNova.ini`（Lua 后端） | **我们的** —— Lyra 没这个 ini，由片段生成 → `Tools/lyra/patches/config/DefaultUnLuaNova.ini.append` |
| 打包一堆样板设置 | Lyra 的 |

追加是幂等的（带 `UAK-PATCH` 标记，重复跑不会叠加）；Lyra 没有的 ini 则由片段直接生成。
片段里如果只剩注释（比如 `DefaultGameplayTags.ini.append` 现在只是占位说明），
`lyra/patch.py` 会跳过它，不会去造一个空 ini。

```bash
python Tools/lyra/cli.py patch --check   # 只看状态
python Tools/lyra/cli.py patch           # 真正打
python Tools/lyra/cli.py patch --revert  # 按 .orig 还原到 Lyra 原版
```

> 一个已知冲突：示例模块用的是**旧式动作映射**，Lyra 用的是 **Enhanced Input**。
> 两者在同一份 ini 里可以共存（重复 section 会被合并），但长期看示例模块应该迁到
> Enhanced Input，见 `docs/examples/05-run-the-demo.md`。

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

## 脚本方案（UnLuaNova）在 Lyra 宿主下的位置

脚本方案是 git submodule（`Plugins/UnLuaNova`，v1.0.0），UE 会递归发现它内层的插件，
`.uproject` 里因此没有任何 UnLuaNova 相关条目。它在 Lyra 宿主里的定位：

| 问题 | 答案 |
|---|---|
| Lyra 需要 Lua 吗 | **不需要**。Lyra 侧的实现全是 C++（例如引导条件 `ULyraAdaptInputCondition` 读 Enhanced Input） |
| 谁在用脚本 | 只有插件自带的示例模块 `UAKDemo`（`AAKDemoCharacter` → `Content/Script/UAKDemoCharacter.lua`） |
| 插件本体依赖它吗 | 不依赖。`UEAbilityKit.uplugin` 里的引用标了 `"Optional": true`，缺了也不拦 |
| 打包要注意什么 | 脚本不是资产，要显式 stage 两条路径（工程脚本 + 插件运行时模块），见 [04-scripting-with-unlua.md](04-scripting-with-unlua.md) §6 |
| 不想要怎么办 | 见 04 §9（关 Build.cs / 角色接口 / 工程脚本 / `.uproject` 开关 / stage 路径） |

内层的 `LuaSocket` / `LuaRapidjson` / `LuaProtobuf` 会被一起发现并编译（它们 `EnabledByDefault=true`），
不需要就在 `.uproject` 里显式关掉。

## 许可与归属（务必保留）

- 拷进来的 Lyra 代码**保持 Lyra 命名**，文件头 Epic 版权声明一个字都别删
- 来源与改动记录在 `Tools/lyra/lyra_manifest.txt` + `THIRD_PARTY/Lyra-origin.md`
- 拷进来哪些文件、每个文件的指纹：`Tools/lyra/.lyra_paths.txt` + `Tools/lyra/.lyra_state.json`（都不进仓库）
- **Lyra 代码只放 `Source/` / `Config/`，绝不进 `Plugins/UEAbilityKit`**

## 常见问题

| 现象 | 原因 |
| --- | --- |
| 编译报找不到 `UEAbilityKit` | `lyra/patch.py` 没跑，或 `LyraGame.Build.cs` 被 `-Force` 覆盖回去了 |
| 所有 Lyra 技能一激活就被取消 | UAK 的"资产为空即 no-op"逻辑没生效，重跑补丁并确认代码 |
| uproject 里我们自己的模块没了 | 补丁只做"补充"不做"删除"；用 `--check` 看，再 `--revert` 后重来 |
| 资产没进工程 | 跑了 `--skip-assets`；重跑一次不带这个参数 |
| `lyra/verify.py` 报"手改" | 有人直接改了拷进来的 Lyra 文件。改法只有一种：把改动写进 `Tools/lyra/lyra_patches.json`，然后 `lyra clean` → `lyra import` |
| `git status` 在导入后不干净 | 外围文件（`.uproject` / `*.Target.cs` / `Platforms/` / `Config/`）与运行期状态都已在 `.gitignore` 里；若仍有多余项，那就是真的手改残留 |
