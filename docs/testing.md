# 测试与自动化

UAK 的目标之一：**让"技能编排"这种容易出玄学 bug 的东西，变成可以被 CI 断言的东西。**

分工是这样的：

| 谁 | 干什么 |
| --- | --- |
| **本地** | 一键跑全套：编译 → 三个 stage → 出结论 + 出截图/录屏/多端一致性 |
| **CI** | 不装 UE、不跑测试；只把本地产出的 `docs/reports` + `docs/showcase.md` 渲染到 GitHub Pages |

测试需要真实 UE 与真实 RHI（要像素才谈得上录屏），放在云端跑既不现实也慢，所以结论**在本地生成后提交回仓库**，CI 只当发布通道。

## 一条命令跑全部

```powershell
# Windows（PowerShell）
./Scripts/run_tests.ps1                 # 全部 stage：Editor + Game + DS/多客户端
./Scripts/run_tests.ps1 -Stage Editor -Filter UAK.Unit -SkipBuild
./Scripts/run_tests.ps1 -Headless       # 没 GPU：只跑断言，不出素材
./Scripts/run_tests.ps1 -Commit         # 跑完把结论提交（CI 随后发布）
```

```bash
# Linux / macOS
./Scripts/run_tests.sh
./Scripts/run_tests.sh --stage editor --filter UAK.Unit --skip-build
./Scripts/run_tests.sh --headless
./Scripts/run_tests.sh --commit
```

脚本做的事：

1. **编译**：`UnrealBuildTool` 编 `UAKDemoEditor`（`-SkipBuild` 可跳过）
2. **跑三个 stage**（见下）
3. **出结论**：`Scripts/report/generate_report.py` 生成 `docs/reports/latest.{md,html,json}`
4. **出素材**：截图、帧序列（有 ffmpeg 时自动合成 mp4）、血缘树文本
5. **出文档**：`docs/showcase.md` 由素材自动生成（展示页不是手写的宣传页）

## 三个 stage

| Stage | 进程 | 机制 | 断言什么 | 素材 |
| --- | --- | --- | --- | --- |
| **Editor** | `UnrealEditor-Cmd` | 编辑器内 Automation（`Automation RunTests UAK`） | 单元 / 实例时序 / GAS | 逐帧截图 → mp4、血缘树文本 |
| **Game** | `UnrealEditor-Cmd -game` | Runtime 控制台命令 `UAK.Auto.*` | 真实游戏世界里技能能跑起来 | 逐帧录屏、GAS 快照 |
| **Net** | 1 个 `-server` + N 个 `-game` | 同上 | **各端属性是否收敛到同一个值** | 各端录屏、快照比对表 |

为什么要两套机制：`UEAbilityKitTests` 是 `Type: Editor` 模块，**在 `-game` / `-server` 进程里不一定加载**，
所以进程外的 stage 不用 Automation 用例，而用 Runtime 模块自带的控制台命令，并在日志里打机器可读的标记：

```
UAK_E2E <Action> <Name> <OK|FAIL|SKIP> [k=v ...]
```

`generate_report.py` 把这些标记还原成用例行（`UAK.E2E.*`），因此**有没有 JSON 报告都能出结论**。

### 进程外能用的命令（`UAK.Auto.*`）

```
UAK.Auto.ActivateAbility <ClassPath> [Period]   # 在本地玩家的 ASC 上激活技能；Period>0 时周期重试
UAK.Auto.Snapshot <Role> [Period]               # 导出 GAS 状态 → Saved/UAKAutomation/Net/<Role>.json
UAK.Auto.Record <Name> [Frames] [Fps]           # 逐帧截图 → Saved/UAKAutomation/<Name>/frames/
UAK.Auto.Screenshot <Name>                      # 单张截图 → Saved/UAKAutomation/<Name>/shot.png
```

DS 进程没有渲染目标，`Record` 会自动 `SKIP`（同步正确性靠 Snapshot 比对，不靠画面）。

### 多客户端怎么算过

`compare_net()` 读 `Net/*.json`，以权威端为基准，逐个属性比对各客户端：

- 不同进程里同一个 Actor 的名字不一样，所以按"**属性值的多重集合**"比对，而不是按 Actor 名字配对
- 浮点容差 `0.01`；数量不一致 / 客户端缺属性都算失败
- 产出 `UAK.Net.Converge.<Role>` 行，以及 md/html 里的一致性表

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
| `UAK.Media.*` | 表现 | 逐帧截图 + 血缘树文本；**没开采集时自动跳过并算通过** |
| `UAK.E2E.*` | 进程外 | Game / Net stage 的 `UAK.Auto.*` 标记还原出来的结论行 |
| `UAK.Net.Converge.*` | 进程外 | 多端属性一致性 |

"实例测试"的原则：**不 mock 任何东西**。
需要世界就建一个临时 `EWorldType::EditorPreview` 世界，需要 GE 就用 C++ 定义的 GE 类
（而不是依赖 `.uasset`），所以没有内容资产也能跑出真实结论。

`UAK.Instance.*` 的时序靠手动 `Instance->Tick(dt)` 推进：确定性、毫秒级、零 flaky；
`UAK.Media.*` 才交给引擎 tick 逐帧采集——**断言层和素材层分开**，谁都不会互相拖累。

## 素材放哪

```
Saved/UAKAutomation/            # 原始产物（Saved/ 已 gitignore，不入库）
  <Test>/frames/*.png           # 帧序列 → ffmpeg 合成 mp4
  <Test>/<name>.txt             # 血缘树
  Net/<Role>.json               # 各端 GAS 快照
docs/reports/media/**           # 只有 mp4 + 少量关键 png 入库（走 LFS，见 .gitattributes）
```

帧序列本身**不入库**，仓库里只留合成后的 mp4。

## 为什么需要 `UEAbilityKitTests` 这个模块

- 它随插件发布，用 UAK 的人在自己的工程里也能跑同一套用例
- 它是 `Type: Editor`，不会进打包产物（也因此进程外 stage 走 `UAK.Auto.*` 而不是它）
- 不想带测试时，从 `UEAbilityKit.uplugin` 里删掉这个模块即可

## CI

- `.github/workflows/docs.yml`：推送到主干时构建 MkDocs 站点（含报告与素材）并部署 GitHub Pages
- ~~`ue-tests.yml`~~ 已删除：测试不在云端跑

## 常见问题

| 现象 | 原因 |
| --- | --- |
| 报告里没有截图/录屏 | 跑了 `-Headless`（`-NullRHI` 没有像素）；或没装 ffmpeg（只有 png 没有 mp4） |
| `UAK.Media.*` 全部通过但没素材 | 同上；用例检测到没开采集会直接跳过 |
| Net stage 只有 1 份快照 | DS 没起来或客户端没连上：看 `Saved/Automation/Logs/Server-*.log`，必要时调大 `-ServerBootWait` |
| 多端一致性失败 | 先看是不是 Actor 数量不一致（有端还没 Possess）；再查 GE 的复制设置，见 `docs/examples/01-minimal-multiplayer-setup.md` |
| mp4 没生成 | `ffmpeg` 不在 PATH；或帧目录是空的 |
