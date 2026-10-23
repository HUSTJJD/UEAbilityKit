# 血缘视图：回答「这 120 伤害是哪来的」

血缘（Explain）是 UAK 最强的差异化能力：不运行游戏就能看到一条技能对 GAS 的全部影响，还能逐层下钻到属性数值。这篇教你读它、用它定位问题。

**难度**：进阶　**预计**：10 min　**步骤**：10

!!! warning "还没有录制产物"
    本页目前只由教程脚本渲染，截图 / 录屏尚未采集。
    在本机执行 `Scripts/generate_tutorials.ps1`（需要 UE 与非 NullRHI）即可补齐素材，
    文档结构与讲解不受影响。

## 先修

- 知道 GameplayEffect 会改属性
- 做过「第一个技能」教程

## 学完自查

- [ ] 我知道血缘视图在「设计期」和「运行期（PIE）」分别由什么数据驱动
- [ ] 我能从血缘树一路下钻到 SetByCaller 与具体目标
- [ ] 我知道 GetGASImpact 是每个任务自己实现的契约

## 步骤一览

| # | 步骤 | 动作 | 状态 |
| --- | --- | --- | --- |
| 1 | [新建并打开技能](#step-1) | `OpenAsset` | ⚪ |
| 2 | [加一条 Effect 轨道](#step-2) | `AddTrack` | ⚪ |
| 3 | [放一个 ApplyGameplayEffect，先不填 GE](#step-3) | `AddTask` | ⚪ |
| 4 | [校验：1 条错误](#step-4) | `Validate` | ⚪ |
| 5 | [补上 GE 类](#step-5) | `SetTaskProp` | ⚪ |
| 6 | [再校验：0 错误](#step-6) | `Validate` | ⚪ |
| 7 | [切到血缘视图](#step-7) | `FocusTab` | ⚪ |
| 8 | [把光标移到 0.35 秒](#step-8) | `SetCursor` | ⚪ |
| 9 | [下钻到任务节点](#step-9) | `SelectTask` | ⚪ |
| 10 | [导出血缘树文本](#step-10) | `Exec` | ⚪ |

---

## 第 1 步 · 新建并打开技能 {: #step-1 }

**为什么这么做**：血缘视图的数据来自每个任务的 `GetGASImpact()`——它是 UAK 任务的四个契约之一（生命周期 / 自校验 / 自述影响 / Json 双向）。因为影响是「任务自己说出来的」，所以**不用跑游戏就能画出来**。

**操作**：新建 Ability Asset，Length 2.0。

**应该看到**：空的资产编辑器。

??? note "自动化是怎么做这一步的"

    **动作**：`OpenAsset`
    
    **引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。
    
    > 这一步还没有录制产物（跑一次 `Scripts/generate_tutorials.ps1` 即可补上）。
    

---

## 第 2 步 · 加一条 Effect 轨道 {: #step-2 }

**为什么这么做**：伤害、Buff、治疗、护盾在 UAK 里全部是 GE——UAK 不自建任何属性 / 修正器系统，这是「不重复造轮子」这条原则的直接体现。

**操作**：工具条 → +轨道 → Effect。

**应该看到**：左栏出现 Effect 轨道。

??? note "自动化是怎么做这一步的"

    **动作**：`AddTrack`
    
    ```json
    {
  "name": "Effect",
  "type": 1
}
    ```
    
    **引导模式**：做到「轨道数 ≥ 1」就自动进入下一步。
    
    > 这一步还没有录制产物（跑一次 `Scripts/generate_tutorials.ps1` 即可补上）。
    

---

## 第 3 步 · 放一个 ApplyGameplayEffect，先不填 GE {: #step-3 }

**为什么这么做**：故意先不填。马上你就会看到校验把它拦下来——这就是「错误无法被配置出来」。

**操作**：右键 → 添加任务 → Apply Gameplay Effect，0.3 – 0.1。

**应该看到**：片段出现，但底部校验条立刻冒出 1 条错误。

??? note "自动化是怎么做这一步的"

    **动作**：`AddTask`
    
    ```json
    {
  "track": "Effect",
  "type": "UAKAbilityTask_ApplyGameplayEffect",
  "startTime": 0.3,
  "duration": 0.1,
  "taskId": "Damage"
}
    ```
    
    **引导模式**：做到「任务数 ≥ 1」就自动进入下一步。
    
    > 这一步还没有录制产物（跑一次 `Scripts/generate_tutorials.ps1` 即可补上）。
    

---

## 第 4 步 · 校验：1 条错误 {: #step-4 }

**为什么这么做**：注意错误来自**任务自己的 Validate()**，不是外部绑定的校验器。新增一个任务类型时，它的规则跟着它一起走，不会出现「任务换了、校验器忘了跟」。

**操作**：看底部校验条，双击条目可直接定位到片段。

**应该看到**：1 条错误：ApplyGameplayEffect 未指定 GE。

??? note "自动化是怎么做这一步的"

    **动作**：`Validate`
    
    **引导模式**：做到「校验错误数 == 1；超时 120s 自动放行」就自动进入下一步。
    
    > 这一步还没有录制产物（跑一次 `Scripts/generate_tutorials.ps1` 即可补上）。
    

---

## 第 5 步 · 补上 GE 类 {: #step-5 }

**为什么这么做**：UAK 里 GE 可以是任意 GE 类：`.uasset` 蓝图 GE、C++ GE 都行。示例工程里 `UAKDemoGE_MeleeDamage` 是一个 C++ GE（Health -30）——用 C++ GE 的好处是仓库里不用塞二进制。

**操作**：Inspector → Effect 分类 → EffectClass 选 GE_MeleeDamage（或你的项目里的伤害 GE）。

**应该看到**：校验条上的错误消失。

??? note "自动化是怎么做这一步的"

    **动作**：`SetTaskProp`
    
    ```json
    {
  "track": "Effect",
  "task": 0,
  "property": "EffectClass",
  "value": "/Script/UAKDemo.UAKDemoGE_MeleeDamage"
}
    ```
    
    **引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。
    
    > 这一步还没有录制产物（跑一次 `Scripts/generate_tutorials.ps1` 即可补上）。
    

---

## 第 6 步 · 再校验：0 错误 {: #step-6 }

**为什么这么做**：改完立刻重算，不用手动点「校验」——常驻底栏的设计就是为了边改边报。

**操作**：看底部校验条。

**应该看到**：0 错误。

??? note "自动化是怎么做这一步的"

    **动作**：`Validate`
    
    **引导模式**：做到「校验错误数 == 0；超时 120s 自动放行」就自动进入下一步。
    
    > 这一步还没有录制产物（跑一次 `Scripts/generate_tutorials.ps1` 即可补上）。
    

---

## 第 7 步 · 切到血缘视图 {: #step-7 }

**为什么这么做**：Able 是「时间轴 + 蓝图图」双模式；UAK 的资产是纯数据资产（不是蓝图），所以没有图模式，取而代之的就是血缘视图。它回答的不是「现在在跑哪个任务」，而是「这 120 点伤害是哪来的」。

**操作**：左栏视图切换 → 血缘（Explain）。

**应该看到**：视图变成一棵树：技能 → 任务 → GE → SetByCaller / 目标。

??? note "自动化是怎么做这一步的"

    **动作**：`FocusTab`
    
    ```json
    {
  "tab": "Bloodline"
}
    ```
    
    **引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。
    
    > 这一步还没有录制产物（跑一次 `Scripts/generate_tutorials.ps1` 即可补上）。
    

---

## 第 8 步 · 把光标移到 0.35 秒 {: #step-8 }

**为什么这么做**：设计期的血缘由 `GetGASImpact()` 静态汇总；运行期（PIE）则由 `UAKTrace` 写入真实数据，能看到**每个目标各自**掉了多少血、谁被格挡了、谁 miss 了。

**操作**：拖动光标到伤害片段中间。

**应该看到**：GAS 影响条与血缘树上都高亮这一段。

??? note "自动化是怎么做这一步的"

    **动作**：`SetCursor`
    
    ```json
    {
  "time": 0.35
}
    ```
    
    **引导模式**：做到「光标 ≥ 0.35s」就自动进入下一步。
    
    > 这一步还没有录制产物（跑一次 `Scripts/generate_tutorials.ps1` 即可补上）。
    

---

## 第 9 步 · 下钻到任务节点 {: #step-9 }

**为什么这么做**：血缘树可以一跳一跳展开：技能 → 任务 → GE → SetByCaller → 目标 → 属性。定位一个「莫名其妙的伤害」，从这棵树往下走三跳就够了。

**操作**：在时间轴里选中伤害任务，再看血缘视图。

**应该看到**：血缘树定位到该任务节点，展开可见 GE 类与 SetByCaller 数值。

??? note "自动化是怎么做这一步的"

    **动作**：`SelectTask`
    
    ```json
    {
  "track": "Effect",
  "task": 0
}
    ```
    
    **引导模式**：做到「已选中一个任务」就自动进入下一步。
    
    > 这一步还没有录制产物（跑一次 `Scripts/generate_tutorials.ps1` 即可补上）。
    

---

## 第 10 步 · 导出血缘树文本 {: #step-10 }

**为什么这么做**：运行期可以用 `UAK.Trace.Dump` 把血缘树打成文本。它是最适合进 CI 的产物——headless 环境没有像素，但文本一定有。

**操作**：控制台执行 UAK.Trace.Dump。

**应该看到**：日志里出现技能 → 任务 → GE → SetByCaller → 目标 → 属性的树形文本。

??? note "自动化是怎么做这一步的"

    **动作**：`Exec`
    
    ```json
    {
  "cmd": "UAK.Trace.Dump"
}
    ```
    
    **引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。
    
    > 这一步还没有录制产物（跑一次 `Scripts/generate_tutorials.ps1` 即可补上）。
    

---

## 这篇文档是怎么来的

```text
Tutorials/<id>.json      ← 人写的：讲解 + 动作 + 采集声明（唯一事实来源）
   ↓ UAK.Tutorial.Run（编辑器内自动化，真实点一遍）
Saved/UAKAutomation/Tutorials/<id>/{run.json, *.png, frames/}
   ↓ Scripts/tutorial/generate_tutorials.py
docs/tutorials/<id>.md   ← 你正在看的这页
```

教程不是「先写文档再补截图」，而是**录制过程即文档**：
每一步的讲解与每一步的动作写在同一个对象里，改了编辑器行为就必须改脚本，
否则 `Automation RunTests UAK.Tutorial` 会失败。

_生成时间：2026-10-22 17:58:19_

