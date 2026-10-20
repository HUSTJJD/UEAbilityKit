# 测试与自动化

UAK 的目标之一：**让"技能编排"这种容易出玄学 bug 的东西，变成可以被 CI 断言的东西。**
所以测试不是附属品，而是与插件一起发布的模块（`UEAbilityKitTests`）。

## 一条命令跑全部

```powershell
# Windows（PowerShell）
./Scripts/run_tests.ps1                 # 编译 + 跑全部 UAK.* 用例 + 生成报告
./Scripts/run_tests.ps1 -Filter UAK.Unit -SkipBuild
./Scripts/run_tests.ps1 -CaptureMedia   # 额外产出截图与录屏（需要真实 RHI）
```

```bash
# Linux / macOS
./Scripts/run_tests.sh
./Scripts/run_tests.sh --filter UAK.Unit --skip-build
./Scripts/run_tests.sh --capture-media
```

脚本做的事：

1. **编译**：`UnrealBuildTool` 编 `UAKDemoEditor`（`-SkipBuild` 可跳过）
2. **跑测试**：`UnrealEditor-Cmd` + `Automation RunTests UAK` + `-ReportExportPath`
   - 默认 `-NullRHI`，headless 环境（含 CI）也能跑
3. **出结论**：`Scripts/report/generate_report.py` 生成
   - `docs/reports/latest.md` / `.html` / `.json`
4. **出素材**：截图、帧序列（有 ffmpeg 时自动合成 mp4）、血缘树文本
5. **出文档**：`docs/showcase.md` 由素材自动生成（展示页不是手写的宣传页）

## 用例组织

命名即分层，用 `-Filter` 就能选跑某一层：

| 前缀 | 层级 | 说明 |
| --- | --- | --- |
| `UAK.Unit.Serialization.*` | 单元 | Json 往返、TaskId、复合轨道参数 |
| `UAK.Unit.Validation.*` | 单元 | 校验规则（长度、越界、缺 GE、引导窗口） |
| `UAK.Unit.Plan.*` | 单元 | 执行计划展开（Timeline / Sequence / Parallel / Repeat / StartDelay / 静音轨道） |
| `UAK.Unit.Metadata.*` | 单元 | 任务编辑器元数据、GAS 影响自述 |
| `UAK.Unit.Filter.*` | 单元 | 查询过滤器（与形状解耦） |
| `UAK.Instance.*` | 实例 | 真跑一次 `UAKAbilityInstance`：时序、门控、中断顺序、保证执行、循环、引导、Branch、溯源 |
| `UAK.GAS.*` | 实例 | 真建 ASC 与属性集，验证 GE 落到属性、条件判定 |

"实例测试"的原则：**不 mock 任何东西**。
需要世界就建一个临时 `EWorldType::EditorPreview` 世界，需要 GE 就用 C++ 定义的 GE 类
（而不是依赖 `.uasset`），所以 CI 里没有内容资产也能跑出真实结论。

## 为什么需要 `UEAbilityKitTests` 这个模块

- 它随插件发布，用 UAK 的人在自己的工程里也能跑同一套用例
- 它是 `Type: Editor`，不会进打包产物
- 不想带测试时，从 `UEAbilityKit.uplugin` 里删掉这个模块即可

## CI

- `.github/workflows/docs.yml`：推送到主干时构建 MkDocs 站点（含报告）并部署 GitHub Pages
- `.github/workflows/ue-tests.yml`：在自托管 runner（装了 UE）上手动触发完整编译 + 测试

GitHub 托管 runner 装不了 UE，所以"编译 + 跑用例"这一步放在自托管 runner；
报告与 Pages 的发布在托管 runner 上完成。
