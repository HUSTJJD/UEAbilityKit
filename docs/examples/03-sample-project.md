# 示例工程结构（对照 UnLua）

## 1. 目录布局

本仓库**本身就是一个可打开的 UE 工程**，UAK 作为插件存在——
与 [UnLua](https://github.com/Tencent/UnLua) 的组织方式一致：项目即示例，库放 Plugins。

```
UEAbilityKit/                       ← 仓库根 = UE 工程根
├─ UAKDemo.uproject                ← 示例工程
├─ Source/
│  ├─ UAKDemo/                     ← 示例游戏模块（属性集 / 角色 / 技能）
│  ├─ UAKDemo.Target.cs
│  ├─ UAKDemoEditor.Target.cs
│  └─ UAKDemoServer.Target.cs
├─ Plugins/
│  └─ UEAbilityKit/                ← UAK 插件（本仓库的主角）
│     ├─ UEAbilityKit.uplugin
│     └─ Source/
│        ├─ UEAbilityKit/          ← Runtime
│        └─ UEAbilityKitEditor/    ← Editor
├─ Config/                          ← 工程配置（最小）
├─ Content/                         ← 内容目录（不提交二进制，见 Content/README.md）
├─ docs/                            ← 设计文档与示例说明
├─ README.md
├─ NOTICE
└─ LICENSE
```

对比 UnLua：

| UnLua | UAK |
|---|---|
| `UnLuaNovaDemo.uproject` | `UAKDemo.uproject` |
| `Source/UnLuaNovaDemo/`（示例游戏代码） | `Source/UAKDemo/`（示例游戏代码） |
| `Plugins/UnLuaNova/`（库） | `Plugins/UEAbilityKit/`（库） |
| `Plugins/UnLuaNovaTestSuite/` | ——（暂无，CI 用 `UAKValidateCommandlet` 代替） |
| `Docs/` | `docs/` |

## 2. 示例模块里有什么

| 文件 | 内容 |
|---|---|
| `Source/UAKDemo/UAKDemoAttributeSet.h/.cpp` | Health / MaxHealth / Mana / MaxMana，含属性复制与夹取 |
| `Source/UAKDemo/UAKDemoCharacter.h/.cpp` | 自带 ASC + 属性集，`PossessedBy` 时授予默认技能 |
| `Source/UAKDemo/UAKDemoAbility.h/.cpp` | `UAKGameplayAbility` 的最小子类，`CommitAbility` 走冷却与消耗 |

## 3. 怎么用

```bash
git clone https://github.com/HUSTJJD/UEAbilityKit.git
```

1. 用 UE 打开 `UAKDemo.uproject`
   - 若使用**源码版引擎**，把 `EngineAssociation` 改成 `""`
2. 按 [Content/README.md](https://github.com/HUSTJJD/UEAbilityKit/blob/main/Content/README.md) 的步骤创建资产
3. 两个客户端 + Listen Server 跑起来验证预测 / 冷却 / 消耗

只想用插件、不要示例工程：把 `Plugins/UEAbilityKit` 整个目录拷进你自己的项目 `Plugins/` 即可，
`Source/UAKDemo`、`Config`、`Content` 都不需要。

## 4. 迁移记录

示例最初是 `Samples/UAKSample` 独立插件，后按"项目即示例"的思路
用 `git mv` 迁成工程模块 `Source/UAKDemo`，类名同步改为 `UAKDemo*` / `AAKDemo*`。
历史保留在 git 记录里。
