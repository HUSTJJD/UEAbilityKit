# 设计原则：AbilityKit 的精髓，以及与 Able 的刻意差异化

> 本文回答两个问题：
> 1. 我们从 AbilityKit（MIT，Unity / 纯 C#）身上到底学到了什么？
> 2. 在哪些地方我们必须**刻意和 Able 做得不一样**（它是商业插件）？

---

## 一、从 AbilityKit 学到的精髓

AbilityKit 的代码一行都用不上（C# / Unity），真正有价值的是它**拆分问题的方式**。

### 1. 三层边界：机制层 / 项目策略层 / 宿主层

```
宿主层（Unity Root / Console / ET / Orleans）
   ↓
项目策略层（组合根、配置规范、系统顺序、权威模型、表现投影）  ← 框架不决定
   ↓
机制层（Pipeline / Triggering / Combat / World / Sync / Tooling）
```

**精髓**：框架只稳定"可复用的机制"，绝不强行抽象一套"所有游戏都必须实现的 BattleApplication"。
**对 UAK 的约束**：我们只做**编排机制 + 编辑器**。英雄规则、房间流程、配置表规范、AI 决策、表现策略一律由项目拥有。
**反面清单**：不做"一键可玩的完整战斗框架"，不做"统一 BattleApplication"，不做"必须继承的基类体系"。

### 2. Pipeline 与 Triggering 是两个正交维度

- **Pipeline** 回答"流程**什么时候**推进"（顺序 / 并行 / 等待 / 分支 / 重复 / 时间轴）。
- **Triggering** 回答"**事件发生后**执行哪些规则和动作"（事件 → 条件 → 强类型 Action → 服务）。

这两个维度在 AbilityKit 里是**分开的两个包**，可以被任意组合。
**对 UAK 的约束**：时间轴只负责"何时"；被动、命中、Buff Tick、区域进出这类"事件驱动"逻辑走独立的规则系统，不塞进时间轴。

### 3. 强类型 Action Schema

配置化的动作最终落到**带参数 schema、服务解析、失败原因、日志**的运行时代码，而不是散落的字符串脚本。
**对 UAK 的约束**：Task 是 **C++ `UObject` + 强类型 `USTRUCT` 参数**，引擎反射可直接序列化与校验；**不做字符串脚本，不做 Lua 开关**。

### 4. Trace：来源血缘可反查

技能释放产生的投射物、区域、Buff、伤害保留 `root / parent / owner` 上下文，
能反查"哪个技能 → 哪个触发器 → 哪个 Action → 哪个目标"。
**对 UAK 的约束**（M4）：`UAKTrace` 是一等公民。这是"能解释战斗结果"的前提，也是调试与回放的基础。

### 5. 持续效果由"规则"治理，而非"计时器"

AbilityKit 的 `continuous` 模块：持续效果的激活 / 阻止 / 暂停 / 恢复 / 移除，由条件规则驱动。
**对 UAK 的约束**：在 GAS 世界里这件事已经解决了——用 **Infinite/Duration GE + GameplayTag 规则**
（`OngoingTagRequirements`、`RemovalTagRequirements`、`GrantedTags`、Stacking 策略）。**不自建 Continuous 运行时**。

### 6. 参数可 Modifier 化

Buff、装备、天赋、场景状态可以动态改写技能 / 投射物 / 召唤物参数，避免"复制出 N 份相似技能配置"。
**对 UAK 的约束**：用 GE 的 Modifier / `SetByCaller` / Attribute-based float。**不自建修正器系统**。

### 7. 可读配置 ↔ 运行时双向

Trigger Plan 既能执行，也能回导为适合审查维护的 Source 配置，**降低配置黑盒化风险**。
**对 UAK 的约束**（M3）：资产可导出为文本（Json）进 Git，且可反向导入；CI 能校验。

### 8. 同步友好的事件模型

事件既能被业务系统消费，也能被快照服务读取，**网络同步逻辑不侵入模拟核心**。
**对 UAK 的约束**：UAK 的逻辑层**完全不碰网络**。网络交给 GAS（`FPredictionKey`、ASC 复制、GE 复制）与 UE 网络层。

### 9. "存在实现 ≠ 生产就绪"

AbilityKit 用 E0–E5 证据等级标注成熟度，明确反对"有代码就宣称可用"。
**对 UAK 的约束**：`docs/roadmap.md` 中每个能力标注状态；README 不用"存在实现"代替"生产就绪"。

### 10. 按需组合，而非全量引入

**对 UAK 的约束**：模块可裁剪——只要数据资产与运行时、不要编辑器，是可行的。

---

## 二、与 Able 的刻意差异化

> 立场：**思想与通用交互模式不受版权保护，受保护的是具体表达**（源码、资源、文档文案、UI 的独创性组合）。
> 因此：时间轴、轨道、验证器、预览视口这类**业界通用形态**可以用；
> Able 的**类结构、命名、数据结构、资源、文案**一律不碰，并且在下面每一处都刻意换一种做法。

| 维度 | Able 的做法 | **UAK 的刻意不同** | 为什么不这么抄 |
|---|---|---|---|
| 资产形态 | `UAblAbility` 是 **Blueprint 资产** | `UAKAbilityAsset : UPrimaryDataAsset` **纯数据资产** | 可文本化导出、可 diff、可批量校验、不依赖蓝图编译 |
| Task 时间语义 | `CanStart = now >= StartTime`、`IsDone = now >= EndTime`，**隐式**由时间比较判定 | Task 显式声明 `StartTime / Duration`，由**执行器**调度 `OnTaskStart/Tick/End` | 语义显式、可做帧锁定、便于静态校验与依赖分析 |
| 分支 | `UAblBranchTask + UAblBranchCondition`，**分支塞在时间轴里** | 时间轴只管"何时"；条件与规则走 **Trigger 规则系统 + GAS Tag 条件** | 职责正交，避免时间轴里长出条件树（Able 的常见痛点） |
| 冷却 / 消耗 / 堆叠 | **自建** | 全部交给 GAS：`CooldownGE` / `CostGE` / Stacking / `GrantedTags` | 可预测、可复制、不重复造轮子 |
| GAS 关系 | 只用 `FGameplayTagContainer`，**不接入 GAS** | **GAS 一等公民**：`UGameplayAbility` / `UGameplayEffect` / `UGameplayCue` / `UAttributeSet` / Prediction | 这是我们的核心差异化，也是 Able 最大的短板 |
| Task 实现 | 蓝图 / **Lua 开关**重依赖 | **C++ 强类型 Task 为主**，蓝图仅作可扩展点 | 性能、可校验、可反射序列化、可静态分析 |
| 时间模型 | 按秒存储，帧只在 UI 层对齐 | 秒为主 + 显式保留"帧锁定模式"的设计位 | 为确定性与回放留口子 |
| 网络 | 只同步"当前激活 Ability 指针"（按 AbilityNameHash 匹配），**无预测回滚** | 依赖 GAS 预测 + ASC 复制；**UAK 逻辑层不碰网络** | 真正的多人可用 |
| ECA 机制 | `UAblAbilityECAModule`：Event/Condition 单槽 + ActionTrue/False/Both 三组 Task | 借鉴 **"事件 → 条件 → 动作"这一思想**，但实现为独立的 Trigger 规则 + 强类型 Action，**不照搬 ECA 的类结构与字段** | 思想无版权；结构与字段照搬才有风险 |
| 排序体系 | `OrderValue` / `UID` / 依赖表 / `Realm` **多套体系并存** | 只用 **"轨道 + 时间"** 一套概念 | 概念收敛，降低心智负担 |
| 调试 | `AbleDebugger`：运行时 Billboard + UMG 列表 | 运行时调试 + **`UAKTrace` 血缘导出 + Explain（可解释）** | Able 只能"看到在跑什么"，我们要能"解释为什么" |
| 校验 | 事后 Validator（Log/Warning/Error） | **强类型从源头减少错误** + Validator + 命令行 CI | 与其事后报错，不如让错误无法被配置出来 |

### 额外的规避动作（写进 `NOTICE` 并在代码中遵守）

1. 不使用 `abl` / `Abl` 前缀，不复制任何 Able 的类名与类结构。
2. 不使用 Able 的 `Content/*.uasset`、图片、文档文案。
3. **不提供 Able 的迁移 / 导入工具**（不做 "Able → UAK" 转换），避免形成替代关系与争议。
4. 编辑器 UI 采用通用布局（工具栏 / 轨道 / 视口 / 详情），但具体实现自研；不模仿 Able 独有的交互组合与图标。

---

## 三、UAK 自己的原创点（不属于任何一方）

1. **GAS 原生的时间轴**：GE / GameplayCue / Tag / Montage 是**一等轨道**，一份资产的执行结果天然落在 GAS 的预测与复制之上——Able 不做 GAS，AbilityKit 不在 UE。
2. **可解释的战斗链路（Explain）**：`UAKTrace` 让一次施放能反查"技能 → Task → GE → 目标 → 最终数值"，把 AbilityKit 的溯源思想落进 GAS 世界。
3. **文本化资产 + CI 校验**：解决"技能配置是二进制黑盒"这一 Able 用户真实的长期痛点（Able 被迫外挂 Json + Git 双轨）。
4. **秒 / 帧双时间模型**：为确定性、回放与自动化回归留设计位，而不是事后补。

---

## 四、一句话设计底线

> **学 AbilityKit 的"拆分方式"，用 GAS 的"原语"，做 Able 没做且不便做的"编排 + 溯源 + 可校验"；
> 凡是 Able 已经形成具体表达的部分，一律换一种做法，不借鉴其结构与命名。**
