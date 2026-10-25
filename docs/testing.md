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
./Tools/run_tests.ps1                 # 全部 stage：Editor + Game + DS/多客户端
./Tools/run_tests.ps1 --stage editor --filter UAK.Unit --skip-build
./Tools/run_tests.ps1 --headless       # 没 GPU：只跑断言，不出素材
./Tools/run_tests.ps1 --commit        # 跑完把结论提交（CI 随后发布）
```

```bash
# Linux / macOS
./Tools/run_tests.sh
./Tools/run_tests.sh --stage editor --filter UAK.Unit --skip-build
./Tools/run_tests.sh --headless
./Tools/run_tests.sh --commit
```

脚本做的事：

1. **编译**：`UnrealBuildTool` 编 `LyraEditor`（`--skip-build` 可跳过）
2. **跑三个 stage**（见下）
3. **出结论**：`Tools/test/report/generate_report.py` 生成 `docs/reports/latest.{md,html,json}`
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
| `UAK.Tutorial.*` | 教程 | 教程脚本的静态可执行性（引用的轨道 / 任务 / 属性是否存在，校验数是否与脚本一致） |

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

## 教程流水线（文档也是自动化的产物）

文档会腐化，教程尤其会：改了任务的属性名、删了某个任务类型、教程前后步骤对不上，
**没人会发现**，直到有读者照着点失败。所以 UAK 把教程也做成了流水线的一环：

一条教程脚本喂出**两条链路**，共用一个事实来源：

| 链路 | 命令 | 谁在动手 | 产出 |
| --- | --- | --- | --- |
| 录制 | `UAK.Tutorial.Run` | 引擎**替你点** | 截图 / 录屏 → `docs/tutorials/*.md` |
| 引导 | `UAK.Tutorial.Guided` | **你自己点**，引擎判定进度 | 编辑器内的分步引导（高亮 + 提示） |

| 谁 | 干什么 |
| --- | --- |
| `Tutorials/*.json` | 人写的唯一事实来源：每步的**讲解**（say / do / expect）+ **动作**（action / args）+ **采集声明**（capture）+ **完成条件**（advance） |
| `UAK.Tutorial.Run` | 编辑器内自动化，把教程**真实点一遍**：建资产、加轨道、加任务、改属性、播、校验、导出；同时逐步骤截图 / 采帧序列 |
| `UAK.Tutorial.Guided` | 把同一份脚本转成引擎原生的引导教程（运行时构造 `UEditorTutorial`，不落盘 `.uasset`），按 `advance` 判定读者做对了没 |
| `generate_tutorials.py` | 把「脚本 + 录制产物」渲染成 `docs/tutorials/*.md`：分步讲解 + 截图 + mp4 + 每步的执行结果 |
| `UAK.Tutorial.Scripts` | Automation 用例：不打开任何窗口，纯数据层按脚本**模拟一遍**（建轨道 / 建任务 / 把 props 与 value 真的写进去 / 跑校验），断言每一步引用的轨道 / 任务 / 属性都真实存在，且 `Validate` 步骤的错误数与脚本声明的 `expectErrors` 一致 |

同源，所以**录制过程即文档、引导过程也出自同一份脚本**：改了编辑器行为就必须改脚本，否则用例会失败，两条链路也同时对不上。

### 引导模式怎么判定"做对了"

`advance` 只声明**目标状态的阈值**，不声明操作序列：

```json
{ "id": "add-main-track", "action": "AddTrack",
  "advance": { "minTracks": 1, "timeout": 120 } }
```

读者可以先删了重加、可以先加任务后加轨道，只要最终状态对得上就放行。这是引导与录制的分界线：
录制要求严格按脚本走，引导只要求走到该到的地方。

`timeout` 是**必须**给的：条件万一因版本差异永远无法满足，读者会被永久卡住——宁可放行（可能没做对），也不能让人走不下去。没写 `advance` 的步骤仍由读者点「下一步」。

```powershell
./Tools/generate_tutorials.ps1                  # 录制全部教程并生成文档
./Tools/generate_tutorials.ps1 --docs-only        # 只用已有产物重新生成文档
./Tools/generate_tutorials.ps1 --no-capture       # 只验证步骤能不能跑通
./Tools/generate_tutorials.ps1 --tutorial 01-first-ability
```

```bash
./Tools/generate_tutorials.sh --docs-only
```

### 编辑器内的命令（`UAK.Tutorial.*`）

```
UAK.Tutorial.Run <脚本路径|教程 id>    跑一条教程
UAK.Tutorial.RunAll [目录] [-Quit]     跑目录下全部教程；-Quit 表示跑完自动退出进程
UAK.Tutorial.List [目录]               列出可用教程
UAK.Tutorial.Stop                      中断（已跑完的步骤照样写进 run.json）
UAK.Tutorial.Capture 0|1               关 / 开素材采集（关掉就是"只验证步骤"）

UAK.Tutorial.Guided <教程id|脚本路径>   启动交互引导（读者自己动手）
UAK.Tutorial.GuidedStop                结束当前交互引导
```

### 引导模式借用的是引擎自带的机制

`UAK.Tutorial.Guided` 没有另起一套 UI，而是复用引擎的 `IntroTutorials`
（5.8 里在插件 `Engine/Plugins/Editor/GuidedTutorials`，模块名仍叫 `IntroTutorials`）：

- 教程对象**运行时 `NewObject<UEditorTutorial>`**，不落盘成 `.uasset`——
  内容始终以 JSON 为准，随时可重算，也不会给仓库塞二进制；
- 高亮锚点用官方的 `NamedWidget`，它匹配的是 `SWidget::GetTag()`，是 Slate 通用机制，
  所以 UAK 自己的面板只要打个 `UAK.Timeline` / `UAK.Bloodline` 之类的 Tag 就能被框住；
- 依赖做了**三层降级**：编译期探测不到 `IntroTutorials` 源码就不编进来
  （`UAK_WITH_INTRO_TUTORIALS=0`）；运行期插件没启用则 `Start()` 明确报错并给出替代方案；
  使用期步骤默认仍由读者点「下一步」。任何一层不满足，UAK 本体都照常工作。

产物落在 `Saved/UAKAutomation/Tutorials/<tutorial>/`：

```
run.json                      每步的执行结果（状态 / 耗时 / 校验数 / 素材路径）
step_01_<id>.png              步骤截图
step_01_<id>/frames/*.png     帧序列 → ffmpeg 合成 mp4
export/*.json                 教程里导出的资产 Json
```

### 为什么用 `UnrealEditor.exe` 而不是 `-Cmd`

教程要截的是**资产编辑器面板**（时间轴 / 血缘 / Inspector），不是视口，
所以走 `FWidgetRenderer` 把 Slate 控件直接渲到 RenderTarget——与窗口大小、屏幕分辨率无关，
确定性远高于截屏。但 `-NullRHI` 下没有像素，headless 环境只能跑「只验证」模式。

## CI

- `.github/workflows/docs.yml`：推送到主干时构建 MkDocs 站点（含报告与素材）并部署 GitHub Pages
- ~~`ue-tests.yml`~~ 已删除：测试不在云端跑

## 常见问题

| 现象 | 原因 |
| --- | --- |
| 报告里没有截图/录屏 | 跑了 `--headless`（`-NullRHI` 没有像素）；或没装 ffmpeg（只有 png 没有 mp4） |
| `UAK.Media.*` 全部通过但没素材 | 同上；用例检测到没开采集会直接跳过 |
| Net stage 只有 1 份快照 | DS 没起来或客户端没连上：看 `Saved/Automation/Logs/Server-*.log`，必要时调大 `-ServerBootWait` |
| 多端一致性失败 | 先看是不是 Actor 数量不一致（有端还没 Possess）；再查 GE 的复制设置，见 `docs/examples/01-minimal-multiplayer-setup.md` |
| mp4 没生成 | `ffmpeg` 不在 PATH；或帧目录是空的 |
| 教程页只有文字没有图 | 没跑录制（`--docs-only` 只是重新渲染）；或 `-NullRHI` 没有像素 |
| 教程某一步 FAIL | 看该步折叠块里的备注；最常见是 `props` 里的属性名或任务类型改过了 |
