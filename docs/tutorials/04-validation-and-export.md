# 校验治理与 Json 往返：让错误无法被配置出来

校验不是「保存时弹个窗」，而是常驻底栏、边改边报；导出的 Json 也不是给人看的快照，而是能原样回导、能进 CI 的权威文本。这篇把两件事一次讲透。

**难度**：进阶　**预计**：9 min　**步骤**：13

!!! warning "还没有录制产物"
    本页目前只由教程脚本渲染，截图 / 录屏尚未采集。
    在本机执行 `Tools/generate_tutorials.ps1`（需要 UE 与非 NullRHI）即可补齐素材，
    文档结构与讲解不受影响。

## 先修

- 知道 GE 与 GameplayCue 的分工
- 知道 git diff 是什么

## 学完自查

- [ ] 我能区分 Error 与 Warning 分别代表什么
- [ ] 我知道双击校验条目可以定位到片段
- [ ] 我知道 Json 是双向无损的，且能被 commandlet 校验

## 步骤一览

| # | 步骤 | 动作 | 状态 |
| --- | --- | --- | --- |
| 1 | [新建并打开技能](#step-1) | `OpenAsset` | ⚪ |
| 2 | [加 Effect 轨道并放一个空壳 GE 任务](#step-2) | `AddTrack` | ⚪ |
| 3 | [空壳 GE 任务已就位](#step-3) | `AddTask` | ⚪ |
| 4 | [再放一个空壳 Cue 任务](#step-4) | `AddTrack` | ⚪ |
| 5 | [空壳 Cue 任务已就位](#step-5) | `AddTask` | ⚪ |
| 6 | [校验：2 条错误](#step-6) | `Validate` | ⚪ |
| 7 | [补上 GE 类](#step-7) | `SetTaskProp` | ⚪ |
| 8 | [补上 Cue Tag](#step-8) | `SetTaskProp` | ⚪ |
| 9 | [校验：0 错误](#step-9) | `Validate` | ⚪ |
| 10 | [故意把技能长度改小，制造警告](#step-10) | `SetAssetProp` | ⚪ |
| 11 | [校验：0 错误 + 若干警告](#step-11) | `Validate` | ⚪ |
| 12 | [导出 Json](#step-12) | `ExportJson` | ⚪ |
| 13 | [它在 CI 里的位置](#step-13) | `Note` | ⚪ |

---

## 第 1 步 · 新建并打开技能 {: #step-1 }

**为什么这么做**：校验条常驻在编辑器**底部**，不是弹窗。理由很直接：弹窗是「你来问我才答」，常驻条是「边改边报」——错误在写出来的那一秒就出现，而不是等到保存。

**操作**：新建 Ability Asset，Length 2.0。

**应该看到**：底部出现常驻校验条（当前显示 0 警告 · 0 错误）。

??? note "自动化是怎么做这一步的"

    **动作**：`OpenAsset`
    
    **引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。
    
    > 这一步还没有录制产物（跑一次 `Tools/generate_tutorials.ps1` 即可补上）。
    

---

## 第 2 步 · 加 Effect 轨道并放一个空壳 GE 任务 {: #step-2 }

**为什么这么做**：我们先把两个「必定出错」的任务摆上去，看看校验怎么说。

**操作**：工具条 → +轨道 → TrackName = Effect，TrackType = Effect。

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
    
    > 这一步还没有录制产物（跑一次 `Tools/generate_tutorials.ps1` 即可补上）。
    

---

## 第 3 步 · 空壳 GE 任务已就位 {: #step-3 }

**为什么这么做**：这条错误不是外部校验器报的，是 `UAKAbilityTask_ApplyGameplayEffect::Validate()` 自己报的。新增任务类型时规则跟着任务走，不会出现「任务换了、校验器忘了跟」的漂移。

**操作**：确认片段在 Effect 轨道上。

**应该看到**：片段可见。

??? note "自动化是怎么做这一步的"

    **动作**：`AddTask`
    
    ```json
    {
  "track": "Effect",
  "type": "UAKAbilityTask_ApplyGameplayEffect",
  "startTime": 0.2,
  "duration": 0.1,
  "taskId": "Unfilled"
}
    ```
    
    **引导模式**：做到「任务数 ≥ 1」就自动进入下一步。
    
    > 这一步还没有录制产物（跑一次 `Tools/generate_tutorials.ps1` 即可补上）。
    

---

## 第 4 步 · 再放一个空壳 Cue 任务 {: #step-4 }

**为什么这么做**：表现类任务同样有自己的规则：Cue Tag 为空就是「这个任务什么也不会触发」，属于错误而不是警告。

**操作**：工具条 → +轨道 → TrackName = Presentation，TrackType = Presentation。

**应该看到**：左栏出现第二条轨道。

??? note "自动化是怎么做这一步的"

    **动作**：`AddTrack`
    
    ```json
    {
  "name": "Presentation",
  "type": 2
}
    ```
    
    **引导模式**：做到「轨道数 ≥ 2」就自动进入下一步。
    
    > 这一步还没有录制产物（跑一次 `Tools/generate_tutorials.ps1` 即可补上）。
    

---

## 第 5 步 · 空壳 Cue 任务已就位 {: #step-5 }

**为什么这么做**：现在两条错误分别来自两个不同的任务。双击任意一条都会选中并滚动到对应片段——**定位成本必须接近零**，否则没人会用校验。

**操作**：右键 Presentation 轨道 → 添加任务 → Send Gameplay Cue，0.25 – 0.0，不填 Cue Tag。

**应该看到**：片段可见，校验条变成 2 条错误。

??? note "自动化是怎么做这一步的"

    **动作**：`AddTask`
    
    ```json
    {
  "track": "Presentation",
  "type": "UAKAbilityTask_SendGameplayCue",
  "startTime": 0.25,
  "duration": 0.0,
  "taskId": "UnfilledCue"
}
    ```
    
    **引导模式**：做到「任务数 ≥ 2」就自动进入下一步。
    
    > 这一步还没有录制产物（跑一次 `Tools/generate_tutorials.ps1` 即可补上）。
    

---

## 第 6 步 · 校验：2 条错误 {: #step-6 }

**为什么这么做**：这一刻的「2 条错误」是可以被引擎断言的——教程流水线会把这个数字写进产物，`Automation RunTests UAK.Tutorial` 会验证它。教程因此不会教人做一件做不到的事。

**操作**：看底部校验条。

**应该看到**：2 条错误，分别指向两个任务。

??? note "自动化是怎么做这一步的"

    **动作**：`Validate`
    
    **引导模式**：做到「校验错误数 == 2；超时 120s 自动放行」就自动进入下一步。
    
    > 这一步还没有录制产物（跑一次 `Tools/generate_tutorials.ps1` 即可补上）。
    

---

## 第 7 步 · 补上 GE 类 {: #step-7 }

**为什么这么做**：示例工程自带几个 C++ GE（`UAKDemoGE_MeleeDamage` / `FireballDamage` / `Cooldown` / `Cost`）。用 C++ GE 的好处：仓库里不塞二进制，冷却 Tag 也不用改 .ini。

**操作**：选中 GE 任务 → Inspector → EffectClass 选 GE_MeleeDamage。

**应该看到**：1 条错误消失。

??? note "自动化是怎么做这一步的"

    **动作**：`SetTaskProp`
    
    ```json
    {
  "track": "Effect",
  "task": 0,
  "property": "EffectClass",
  "value": "/Script/UAKDemo.AKDemoGE_MeleeDamage"
}
    ```
    
    **引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。
    
    > 这一步还没有录制产物（跑一次 `Tools/generate_tutorials.ps1` 即可补上）。
    

---

## 第 8 步 · 补上 Cue Tag {: #step-8 }

**为什么这么做**：Cue Tag 是 GAS 的 GameplayCue Tag。表现与逻辑分离在这里落地：伤害走 GE，火花走 Cue，两边互不干涉。

**操作**：选中 Cue 任务 → Inspector → CueTag 填 GameplayCue.Demo.Hit。

**应该看到**：第 2 条错误消失。

??? note "自动化是怎么做这一步的"

    **动作**：`SetTaskProp`
    
    ```json
    {
  "track": "Presentation",
  "task": 0,
  "property": "CueTag",
  "value": "GameplayCue.Demo.Hit"
}
    ```
    
    **引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。
    
    > 这一步还没有录制产物（跑一次 `Tools/generate_tutorials.ps1` 即可补上）。
    

---

## 第 9 步 · 校验：0 错误 {: #step-9 }

**为什么这么做**：错误清零。注意「错误」和「警告」的分工：**错误**是「这样配一定不对」，**警告**是「这样配可能不是你想要的」（例如任务超出技能长度）。

**操作**：看底部校验条。

**应该看到**：0 错误。

??? note "自动化是怎么做这一步的"

    **动作**：`Validate`
    
    **引导模式**：做到「校验错误数 == 0；超时 120s 自动放行」就自动进入下一步。
    
    > 这一步还没有录制产物（跑一次 `Tools/generate_tutorials.ps1` 即可补上）。
    

---

## 第 10 步 · 故意把技能长度改小，制造警告 {: #step-10 }

**为什么这么做**：把长度改成 0.25 秒，原来的任务就超出了时间轴末端。这类情况被判为**警告而不是错误**——它可能被有意用来做「尾部溢出但仍在跑」的编排，UAK 不该替你决定。

**操作**：工具条 → 资产属性 → Length = 0.25。

**应该看到**：校验条出现「任务结束时间超出技能长度」的警告。

??? note "自动化是怎么做这一步的"

    **动作**：`SetAssetProp`
    
    ```json
    {
  "property": "Length",
  "value": "0.25"
}
    ```
    
    **引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。
    
    > 这一步还没有录制产物（跑一次 `Tools/generate_tutorials.ps1` 即可补上）。
    

---

## 第 11 步 · 校验：0 错误 + 若干警告 {: #step-11 }

**为什么这么做**：错误数依然是 0，说明资产可以正常保存与运行；警告提醒你去确认这是有意为之。

**操作**：看底部校验条。

**应该看到**：0 错误，至少 1 条警告。

??? note "自动化是怎么做这一步的"

    **动作**：`Validate`
    
    **引导模式**：做到「校验错误数 == 0；超时 120s 自动放行」就自动进入下一步。
    
    > 这一步还没有录制产物（跑一次 `Tools/generate_tutorials.ps1` 即可补上）。
    

---

## 第 12 步 · 导出 Json {: #step-12 }

**为什么这么做**：导出的是**强类型字段**，不是属性表 dump。字段名稳定，适合 review 与 diff；每个任务带 `$type`，回导时能定位到具体类。

**操作**：工具条 → 导出 Json。

**应该看到**：得到一个含 version / length / tracks[] / tasks[] 的文本文件。

??? note "自动化是怎么做这一步的"

    **动作**：`ExportJson`
    
    ```json
    {
  "path": "validation.json"
}
    ```
    
    **引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。
    
    > 这一步还没有录制产物（跑一次 `Tools/generate_tutorials.ps1` 即可补上）。
    

---

## 第 13 步 · 它在 CI 里的位置 {: #step-13 }

**为什么这么做**：`UAKValidateCommandlet` 可以直接加载 Json（或资产）跑一遍校验，不需要打开编辑器、不需要 GPU。所以「配置可 diff」不只是口号——它是真的能被流水线断言的。

**操作**：在 CI 里执行 UAKValidateCommandlet；本地则用 Tools/generate_tutorials.ps1 一键重录教程。

**应该看到**：校验结论以退出码返回，失败即阻断。

??? note "自动化是怎么做这一步的"

    **动作**：`Note`
    
    **引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。
    
    > 这一步还没有录制产物（跑一次 `Tools/generate_tutorials.ps1` 即可补上）。
    

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

_生成时间：2026-10-24 00:42:31_

