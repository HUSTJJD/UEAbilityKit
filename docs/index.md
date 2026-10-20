# UEAbilityKit（UAK）

**UEAbilityKit（简称 UAK）**：基于 Unreal Engine 5 与 Gameplay Ability System（GAS）的数据驱动技能编辑器与运行时工具集。

用时间轴编排技能：一段技能 = 一条时间轴 + 若干轨道（Montage / GameplayEffect / GameplayCue / 碰撞检测 / 位移 / 分支），运行时全部落在 GAS 原语上（`UGameplayAbility` / `UGameplayEffect` / `UGameplayCue` / `FGameplayTag` / `UAttributeSet`），不另起炉灶、不重复造轮子。

## 文档导航

- [路线图](roadmap.md) —— 里程碑与当前进度
- **设计文档** —— 设计原则、资产模型、编辑器设计、执行器、目标选取、网络与预测、序列化、校验等
- **示例** —— 最小多人配置、冷却/消耗/堆叠、示例工程
- **调研** —— Able 与 AbilityKit 调研及命名提案

## 设计原则（摘录）

1. **GAS 一等公民**：属性、Buff、修正、冷却、消耗、堆叠、网络预测，一律复用 GAS 原语，不自建。
2. **不重复造轮子**：移动寻路/碰撞用引擎已有能力，只做「技能编排」这一层真正缺失的东西。
3. **数据可 diff**：资产可导出为文本（Json）进 Git，CI 可校验，避免二进制黑盒。
4. **运行时可溯源**：技能 → 投射物 → 区域 → Buff → 伤害保留 root/parent/owner 血缘，便于调试、归因与回放。
5. **逻辑与表现分离**：逻辑走 GE / Tag / Task，表现一律走 `UGameplayCue`。
6. **C++ 优先，蓝图可扩展**：核心 Task 用 C++ `UObject` 实现，允许项目派生蓝图 Task。

## 仓库

- GitHub：[HUSTJJD/UEAbilityKit](https://github.com/HUSTJJD/UEAbilityKit)

> 当前进度：M0–M4 的主体代码已落地（尚未编译验证）。
