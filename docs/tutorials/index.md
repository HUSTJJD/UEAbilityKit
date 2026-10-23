# 编辑器教程

这一组教程**不是手写的**：每一步的讲解、操作与截图/录屏，都由 `UAK.Tutorial.Run` 在真实编辑器里执行一遍后自动产出。

```text
Tutorials/*.json  →  UAK.Tutorial.Run  →  Saved/.../run.json + 截图 + 帧序列
                                        →  generate_tutorials.py  →  docs/tutorials/*.md
```

| 教程 | 难度 | 步骤 | 最近录制 | 状态 |
| --- | --- | --- | --- | --- |
| [第一个技能：从新建资产到跑起来](01-first-ability.md) | 入门 | 13 | - | ⚪ 待录制 |
| [轨道与复合编排：Sequence / Parallel / Repeat](02-tracks-and-composite.md) | 入门 | 11 | - | ⚪ 待录制 |
| [血缘视图：回答「这 120 伤害是哪来的」](03-bloodline-explain.md) | 进阶 | 10 | - | ⚪ 待录制 |
| [校验治理与 Json 往返：让错误无法被配置出来](04-validation-and-export.md) | 进阶 | 13 | - | ⚪ 待录制 |
| [实况联调：让编辑器跟着 PIE 一起动](05-live-debug-pie.md) | 高阶 | 9 | - | ⚪ 待录制 |

## 本地重录一遍

```powershell
./Scripts/generate_tutorials.ps1            # 打开编辑器录制 + 生成文档
./Scripts/generate_tutorials.ps1 -DocsOnly  # 只用已有产物重新生成文档
./Scripts/generate_tutorials.ps1 -NoCapture # 只验证步骤，不采素材
```

```bash
./Scripts/generate_tutorials.sh
./Scripts/generate_tutorials.sh --docs-only
```

完整流程见 [测试与自动化](../testing.md#教程流水线文档也是自动化的产物)。

_生成时间：2026-10-22 17:58:19_

