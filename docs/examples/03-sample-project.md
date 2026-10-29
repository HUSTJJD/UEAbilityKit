# 工程结构（对照 UnLua）

## 1. 目录布局

本仓库**本身就是一个可打开的 UE 工程**，UAK 作为插件存在——
与 [UnLua](https://github.com/Tencent/UnLua) 的组织方式一致：**库放 `Plugins`，示例随库走**。
示例不是被改造进来的某个游戏，而是插件里的 `UAKDemo` 模块。

```
UEAbilityKit/                       ← 仓库根 = UE 工程根
├─ Plugins/
│  ├─ UEAbilityKit/                 ← UAK 插件（本仓库的主角）
│  │  ├─ UEAbilityKit.uplugin
│  │  └─ Source/
│  │     ├─ UEAbilityKit/           ← Runtime（核心：时间轴 / 任务 / GAS 桥接）
│  │     ├─ UEAbilityKitEditor/     ← Editor（资产编辑器）
│  │     ├─ UEAbilityKitTests/      ← 测试
│  │     └─ UAKDemo/                ← 示例模块（随插件分发，不是独立工程）
│  └─ UnLuaNova/                    ← 脚本方案（git submodule，见 04）
│     └─ Plugins/UnLuaNova/         ← 真正的插件嵌在 submodule 的内层
├─ Content/                         ← 内容目录（含 Script/ 示例脚本，见 Content/README.md）
├─ Tools/                           ← 唯一的脚本目录：构建 / 测试 / 教程
├─ docs/
├─ README.md
├─ NOTICE
└─ LICENSE
```

对比 UnLua：

| UnLua | UAK |
|---|---|
| `UnLuaNovaDemo.uproject`（自带示例工程） | 仓库根即工程根，示例随插件走 |
| `Source/UnLuaNovaDemo/`（示例游戏代码） | `Plugins/UEAbilityKit/Source/UAKDemo/`（示例模块，**随插件走**） |
| `Plugins/UnLuaNova/`（库） | `Plugins/UEAbilityKit/`（库） |
| `Plugins/UnLuaNovaTestSuite/` | ——（暂无，CI 用 `UAKValidateCommandlet` 代替） |
| `Docs/` | `docs/` |

## 2. 示例模块里有什么

| 文件 | 内容 |
|---|---|
| `Plugins/UEAbilityKit/Source/UAKDemo/UAKDemoAttributeSet.h/.cpp` | Health / MaxHealth / Mana / MaxMana，含属性复制与夹取 |
| `Plugins/UEAbilityKit/Source/UAKDemo/UAKDemoCharacter.h/.cpp` | 自带 ASC + 属性集，`PossessedBy` 时授予默认技能；**不依赖脚本** —— Lua 绑定由 `Config/DefaultUnLuaNova.ini` 映射过去 |
| `Plugins/UEAbilityKit/Source/UAKDemo/UAKDemoAbility.h/.cpp` | `UAKGameplayAbility` 的最小子类，`CommitAbility` 走冷却与消耗 |

## 3. 怎么用

```bash
git clone --recursive https://github.com/HUSTJJD/UEAbilityKit.git
```

1. 编译：`Tools\build.ps1`
2. 一键生成示例资产 / 蓝图 / 关卡：`-run=UAKDemoSetup`，见 [05-run-the-demo.md](05-run-the-demo.md)
3. 两个客户端 + Listen Server 跑起来验证预测 / 冷却 / 消耗

只想用插件、不要示例：把 `Plugins/UEAbilityKit` 整个目录拷进你自己的项目 `Plugins/` 即可。
`UAKDemo` 会跟着一起进去（它只是插件里的一个模块，不用就不引用），
而 UnLuaNova 是**可选**依赖 —— 没有它插件照样能启用（[04 脚本集成](04-scripting-with-unlua.md)）。

## 4. 迁移记录

1. 示例最初是 `Samples/UAKSample` 独立插件，按"项目即示例"的思路用 `git mv` 迁成工程模块 `Source/UAKDemo`，
   类名同步改为 `UAKDemo*` / `AAKDemo*`
2. `UAKDemo` 降级成插件的一个模块（`UAKDemo.uproject` 与 `*.Target.cs` 已删除），
   任何项目装上插件都能用这套示例
3. 脚本方案从"工程自带"改为 submodule：`Plugins/UnLuaNova`

历史保留在 git 记录里。
