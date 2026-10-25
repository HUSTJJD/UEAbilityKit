# UAK 自动化报告

- 运行时间：`2026-10-24 14:45:20`
- 用例总数：**37**　通过：**36**　失败：**1**

> 本文件由 `Tools/test/report/generate_report.py` 自动生成，请勿手改。
> 测试在本地一键运行（`Tools/run_tests.ps1`），CI 只负责把结论渲染到 Pages。

## 用例明细

| 结果 | 用例 | 耗时(s) |
| --- | --- | --- |
| ✅ | `AlwaysExecuteStillRuns` | 0.011 |
| ✅ | `BuiltinTasksAreAnnotated` | 0.012 |
| ✅ | `CancelsOnFailedCondition` | 0.011 |
| ✅ | `ChannelingWindowIsChecked` | 0.017 |
| ✅ | `CleanAssetHasNoError` | 0.012 |
| ✅ | `DisabledTrackIsSkipped` | 0.013 |
| ✅ | `DisabledTrackSkipped` | 0.013 |
| ✅ | `Distance` | 0.013 |
| ✅ | `DumpsText` | 0.019 |
| ✅ | `EvaluateOnAttribute` | 0.013 |
| ✅ | `FallbackToClassName` | 0.011 |
| ✅ | `FreezesTimeline` | 0.010 |
| ✅ | `GasImpactIsStatic` | 0.014 |
| ✅ | `InvalidLengthIsError` | 0.012 |
| ✅ | `JsonRoundTrip` | 0.013 |
| ✅ | `Lifecycle` | 0.017 |
| ✅ | `MissingEffectClassIsError` | 0.012 |
| ✅ | `ParallelStartsTogether` | 0.019 |
| ✅ | `RecordsFrames` | 0.012 |
| ✅ | `RecordsTaskNodes` | 0.018 |
| ✅ | `RemovedOnInterrupt` | 0.022 |
| ✅ | `RepeatExpandsTrack` | 0.012 |
| ✅ | `Restarts` | 0.013 |
| ✅ | `ReverseStartOrder` | 0.011 |
| ✅ | `RoundTrip` | 0.015 |
| ✅ | `RunsToEnd` | 0.013 |
| ✅ | `Scripts` | 0.029 |
| ✅ | `SequenceChainsTasks` | 0.011 |
| ✅ | `StartDelayShiftsTrack` | 0.018 |
| ✅ | `TaskIdPreserved` | 0.018 |
| ✅ | `TaskOverflowIsReported` | 0.013 |
| ✅ | `TimelineUsesAbsoluteTime` | 0.018 |
| ✅ | `ToSelfWhenNoTarget` | 0.031 |
| ✅ | `ToTarget` | 0.017 |
| ✅ | `TrackCompositePreserved` | 0.015 |
| ❌ | `UAK.Net.Converge.Snapshots` | 0.000 |
| ✅ | `ValidationRules` | 0.012 |

## 失败详情

### `UAK.Net.Converge.Snapshots`
- 只拿到 1 份快照，至少需要 2 份（DS + 客户端）


## 多端一致性（DS / 多客户端）

- 参与的端：`Server`


## 产物

### 血缘树样例（headless 也能产出）
- `media/UAK.Media.Trace/trace.txt`

