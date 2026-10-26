# 轨道与复合编排：Sequence / Parallel / Repeat

轨道不只是「分类分组」。它能整体开关、整体延迟、整体重复——把 AbilityKit 的 Pipeline 组合思想落在了「这一串片段怎么排」上。

**难度**：入门　**预计**：10 min　**步骤**：11

> 本页由 `Tools/tutorial/generate_tutorials.py` 自动生成，素材来自 `UAK.Tutorial.Run` 的真实录制（2026-10-26 20:15:42）。请勿手改。

录制结论：11/11 步通过　引擎 UE5-CL-0

## 先修

- 做过「第一个技能」教程
- 知道什么是前摇 / 命中 / 后摇

## 学完自查

- [ ] 我能说出 Timeline / Sequence / Parallel 三种模式分别什么时候用
- [ ] 我知道轨道整体静音能用来做什么
- [ ] 我知道 Repeat 只在 Sequence / Parallel 下生效

## 步骤一览

| # | 步骤 | 动作 | 状态 |
| --- | --- | --- | --- |
| 1 | [新建并打开一条 3 秒的技能](#step-1) | `OpenAsset` | ✅ |
| 2 | [加一条 Effect 轨道（逻辑）](#step-2) | `AddTrack` | ✅ |
| 3 | [再加一条 Presentation 轨道（表现）](#step-3) | `AddTrack` | ✅ |
| 4 | [在 Effect 上放第一个逻辑任务](#step-4) | `AddTask` | ✅ |
| 5 | [放第二个逻辑任务（故意重叠）](#step-5) | `AddTask` | ✅ |
| 6 | [把 Effect 轨道切成 Sequence](#step-6) | `SetTrackMode` | ✅ |
| 7 | [在表现轨道上放一个 Cue](#step-7) | `AddTask` | ✅ |
| 8 | [把表现轨道切成 Parallel 并重复 3 次](#step-8) | `SetTrackMode` | ✅ |
| 9 | [整体静音表现轨道](#step-9) | `SetTrackMode` | ✅ |
| 10 | [校验：逻辑轨道仍然成立](#step-10) | `Validate` | ✅ |
| 11 | [导出，看 Json 里怎么记这三种模式](#step-11) | `ExportJson` | ✅ |

---

## 第 1 步 · 新建并打开一条 3 秒的技能 {: #step-1 }

**为什么这么做**：这次我们需要更长的时间轴，才能看出「复合排布」和「绝对时间」的差别。

**操作**：新建 Ability Asset，Length 设为 3.0。

**应该看到**：空的时间轴，标尺 0.0 → 3.0。

![新建并打开一条 3 秒的技能](media/02-tracks-and-composite/step_01_open.png)

??? note "自动化是怎么做这一步的"

    **动作**：`OpenAsset`
    
    **引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。
    
    **执行结果**：✅ OK　耗时 0.76s　轨道 0　任务 0
    

---

## 第 2 步 · 加一条 Effect 轨道（逻辑） {: #step-2 }

**为什么这么做**：把「逻辑」和「表现」分到两条轨道，是 UAK 强烈推荐的习惯：分了之后，你才能在不改任何配置的前提下，只跑逻辑做服务器校验。

**操作**：工具条 → +轨道 → TrackName = Effect，TrackType = Effect。

**应该看到**：左栏出现 Effect 轨道。

![加一条 Effect 轨道（逻辑）](media/02-tracks-and-composite/step_02_add-effect-track.png)

??? note "自动化是怎么做这一步的"

    **动作**：`AddTrack`
    
    ```json
    {
  "name": "Effect",
  "type": 1
}
    ```
    
    **引导模式**：做到「轨道数 ≥ 1」就自动进入下一步。
    
    **执行结果**：✅ OK　耗时 0.38s　轨道 1　任务 0
    

---

## 第 3 步 · 再加一条 Presentation 轨道（表现） {: #step-3 }

**为什么这么做**：表现轨道上放 Cue / 特效这类「只在本地端有意义」的任务。它们的 NetMode 默认是 ClientOnly。

**操作**：工具条 → +轨道 → TrackName = Presentation，TrackType = Presentation。

**应该看到**：左栏出现第二条轨道。

![再加一条 Presentation 轨道（表现）](media/02-tracks-and-composite/step_03_add-presentation-track.png)

??? note "自动化是怎么做这一步的"

    **动作**：`AddTrack`
    
    ```json
    {
  "name": "Presentation",
  "type": 2
}
    ```
    
    **引导模式**：做到「轨道数 ≥ 2」就自动进入下一步。
    
    **执行结果**：✅ OK　耗时 0.40s　轨道 2　任务 0
    

---

## 第 4 步 · 在 Effect 上放第一个逻辑任务 {: #step-4 }

**为什么这么做**：我们放一个「挂 Tag」任务当作第一段逻辑。真实项目里这里通常是 ApplyGameplayEffect（伤害 / Buff / 冷却）。

**操作**：右键 Effect 轨道 → 添加任务 → Apply Loose Tags，0.0 – 0.4。

**应该看到**：Effect 轨道上出现第一个片段。

![在 Effect 上放第一个逻辑任务](media/02-tracks-and-composite/step_04_add-effect-a.png)

??? note "自动化是怎么做这一步的"

    **动作**：`AddTask`
    
    ```json
    {
  "track": "Effect",
  "type": "UAKAbilityTask_ApplyLooseTags",
  "startTime": 0.0,
  "duration": 0.4,
  "taskId": "Windup",
  "props": {
    "Tags": "(GameplayTags=((TagName=\"Ability.Windup\")))"
  }
}
    ```
    
    **引导模式**：做到「任务数 ≥ 1」就自动进入下一步。
    
    **执行结果**：✅ OK　耗时 0.39s　轨道 2　任务 1
    

---

## 第 5 步 · 放第二个逻辑任务（故意重叠） {: #step-5 }

**为什么这么做**：注意我把它的起点也设在 0.0，和上一个重叠。**在 Timeline 模式下重叠是完全合法的**——它只是「同一时刻有两件事」。

**操作**：右键 Effect 轨道 → 添加任务 → Wait Until，0.0 – 0.5。

**应该看到**：两个片段在时间上互相重叠。

![放第二个逻辑任务（故意重叠）](media/02-tracks-and-composite/step_05_add-effect-b.png)

??? note "自动化是怎么做这一步的"

    **动作**：`AddTask`
    
    ```json
    {
  "track": "Effect",
  "type": "UAKAbilityTask_WaitUntil",
  "startTime": 0.0,
  "duration": 0.5,
  "taskId": "Hold"
}
    ```
    
    **引导模式**：做到「任务数 ≥ 2」就自动进入下一步。
    
    **执行结果**：✅ OK　耗时 0.37s　轨道 2　任务 2
    

---

## 第 6 步 · 把 Effect 轨道切成 Sequence {: #step-6 }

**为什么这么做**：切到 Sequence 后，任务**忽略自己的绝对时间**，按数组顺序首尾相接：上一个结束，下一个开始。改排布不用再逐个挪时间点——这是它最大的价值。

**操作**：左栏点轨道名 → Inspector → Composite → CompositeMode = Sequence。

**应该看到**：画布上两个片段被重新排成首尾相接，而不是重叠。

![把 Effect 轨道切成 Sequence](media/02-tracks-and-composite/step_06_switch-sequence.png)

??? note "自动化是怎么做这一步的"

    **动作**：`SetTrackMode`
    
    ```json
    {
  "track": "Effect",
  "mode": "Sequence"
}
    ```
    
    **引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。
    
    **执行结果**：✅ OK　耗时 0.38s　轨道 2　任务 2
    

---

## 第 7 步 · 在表现轨道上放一个 Cue {: #step-7 }

**为什么这么做**：表现一律走 GameplayCue，不走 GE 也不走 Task 直连特效——这样才能被 GAS 的网络与预测体系正确对待。

**操作**：右键 Presentation 轨道 → 添加任务 → Send Gameplay Cue，0.2 – 0.0；Cue Tag 填 GameplayCue.Demo.Hit。

**应该看到**：Presentation 轨道上出现一个单帧片段。

![在表现轨道上放一个 Cue](media/02-tracks-and-composite/step_07_add-cue.png)

??? note "自动化是怎么做这一步的"

    **动作**：`AddTask`
    
    ```json
    {
  "track": "Presentation",
  "type": "UAKAbilityTask_SendGameplayCue",
  "startTime": 0.2,
  "duration": 0.0,
  "taskId": "HitSpark",
  "props": {
    "CueTag": "GameplayCue.Demo.Hit"
  }
}
    ```
    
    **引导模式**：做到「任务数 ≥ 3」就自动进入下一步。
    
    **执行结果**：✅ OK　耗时 0.39s　轨道 2　任务 3
    

---

## 第 8 步 · 把表现轨道切成 Parallel 并重复 3 次 {: #step-8 }

**为什么这么做**：Parallel 下所有任务同时开始；配合 RepeatCount=3 / RepeatInterval=0.2，就是「连续三段火花」。**Repeat 只在 Sequence / Parallel 下生效**——Timeline 模式下任务有自己的绝对时间，重复没有意义。

**操作**：选中 Presentation 轨道 → CompositeMode = Parallel，RepeatCount = 3，RepeatInterval = 0.2。

**应该看到**：画布上出现三段落错开的片段。

![把表现轨道切成 Parallel 并重复 3 次](media/02-tracks-and-composite/step_08_switch-parallel.png)

??? note "自动化是怎么做这一步的"

    **动作**：`SetTrackMode`
    
    ```json
    {
  "track": "Presentation",
  "mode": "Parallel",
  "repeatCount": 3,
  "repeatInterval": 0.2
}
    ```
    
    **引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。
    
    **执行结果**：✅ OK　耗时 0.39s　轨道 2　任务 3
    

---

## 第 9 步 · 整体静音表现轨道 {: #step-9 }

**为什么这么做**：左栏轨道头的勾选框就是「整体开关」。关掉表现轨道，执行计划里就不再包含它的任务——服务器校验、压测、跑批时最常用的一个动作，UAK 独有。注意静音**不等于删除**：校验仍然会看它，避免问题被静音藏起来。

**操作**：点 Presentation 轨道头的勾选框，取消勾选。

**应该看到**：该轨道变灰；执行计划里不再包含它的任务。

![整体静音表现轨道](media/02-tracks-and-composite/step_09_mute-presentation.png)

??? note "自动化是怎么做这一步的"

    **动作**：`SetTrackMode`
    
    ```json
    {
  "track": "Presentation",
  "enabled": false
}
    ```
    
    **引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。
    
    **执行结果**：✅ OK　耗时 0.37s　轨道 2　任务 3
    

---

## 第 10 步 · 校验：逻辑轨道仍然成立 {: #step-10 }

**为什么这么做**：静音不等于删除。校验只针对**会参与执行**的轨道，所以被静音轨道上的问题不会来打扰你。

**操作**：看底部校验条。

**应该看到**：0 错误。

![校验：逻辑轨道仍然成立](media/02-tracks-and-composite/step_10_validate.png)

??? note "自动化是怎么做这一步的"

    **动作**：`Validate`
    
    **引导模式**：做到「校验错误数 == 0；超时 120s 自动放行」就自动进入下一步。
    
    **执行结果**：✅ OK　耗时 0.40s　轨道 2　任务 3
    
    > 校验：错误 0 · 警告 1
    
    - WaitUntil 没有配置条件，等价于纯等待（Delay）。
    

---

## 第 11 步 · 导出，看 Json 里怎么记这三种模式 {: #step-11 }

**为什么这么做**：复合参数是轨道级字段（compositeMode / repeatCount / repeatInterval / startDelay），导出后是稳定的文本字段，review 时一眼能看出「这轨是串行的、重复 3 次」。

**操作**：工具条 → 导出 Json。

**应该看到**：Json 里 tracks[i].compositeMode 分别为 Sequence / Parallel。

![导出，看 Json 里怎么记这三种模式](media/02-tracks-and-composite/step_11_export.png)

??? note "自动化是怎么做这一步的"

    **动作**：`ExportJson`
    
    ```json
    {
  "path": "composite.json"
}
    ```
    
    **引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。
    
    **执行结果**：✅ OK　耗时 0.42s　轨道 2　任务 3
    
    - 导出 composite.json
    

---

## 这篇文档是怎么来的

```text
Tutorials/<id>.json      ← 人写的：讲解 + 动作 + 采集声明（唯一事实来源）
   ↓ UAK.Tutorial.Run（编辑器内自动化，真实点一遍）
Saved/UAKAutomation/Tutorials/<id>/{run.json, *.png, frames/}
   ↓ Tools/tutorial/generate_tutorials.py
docs/tutorials/<id>.md   ← 你正在看的这页
```

教程不是「先写文档再补截图」，而是**录制过程即文档**：
每一步的讲解与每一步的动作写在同一个对象里，改了编辑器行为就必须改脚本，
否则 `Automation RunTests UAK.Tutorial` 会失败。

_生成时间：2026-10-26 20:16:00_

