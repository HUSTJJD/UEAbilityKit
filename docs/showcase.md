# Showcase

这一页由 `Tools/test/report/generate_report.py` 自动生成——素材全部来自本地一键跑出来的测试，
**不是手写的宣传页**：截图、录屏、血缘树、多端一致性，都是测试的直接产物。

最近一次运行：`2026-10-24 14:45:20`　（用例 37 个）

## 录屏与截图

本次运行没有产出媒体（headless / `--headless` 运行没有像素）。
在本机执行 `Tools/run_tests.ps1` 即可产出截图与录屏。

## 多端一致性（DS + 多客户端）

| 基准端 | 对比端 | 结论 |
| --- | --- | --- |

## 血缘树（Explain）

UAK 独有的能力：一次施放的完整因果链，可以直接下钻。

### `UAK.Media.Trace/trace.txt`

```text
[Ability] MediaTraceAbility @ 0.000s
  [Task] Traced @ 0.000s
```

