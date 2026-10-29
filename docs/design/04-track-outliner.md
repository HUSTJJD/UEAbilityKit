# 轨道区重设计

> 为什么单开一篇：轨道是技能编辑器里**每天被点几百次**的地方，而我们现在只有"自绘色条 + 名字"。
> 这一篇做三件事：说清我们差在哪、UE 官方（Sequencer / Persona）是怎么做的、我们要改成什么样。
>
> 参照物来自本机引擎源码：`Engine/Source/Editor/Sequencer/**`、`Engine/Source/Editor/Persona/**`（UE5，Sequencer 已完成 MVVM 重构）。

---

## 1. 现状自评：我们差在哪

| 能力 | 我们现在 | Sequencer | Persona（Notify 轨） | 差距 |
|---|---|---|---|---|
| 左右分栏 | 时间轴左 140px 硬编码，全自绘 | `SSplitter`，宽持久化，最小 150 | `SSplitter` + Fill 系数 0.25/0.75 | 🔴 严重 |
| 轨道多了怎么办 | **画不下就没了**，无垂直滚动 | 共享一根 `SScrollBar` | 共享一根 `SScrollBar` | 🔴 严重 |
| 轨道行内容 | 色条 + 名称 | 图标 + 可编辑名 + Mute/Solo/Lock/Add/Color 多列 | 可编辑名 + hover 才显形的下拉 | 🔴 严重 |
| 轨道排序 | 工具条"上移/下移"按钮 | 拖行重排 + 缩进插入线 | Insert/Remove + 拖拽 | 🔴 严重 |
| 轨道右键菜单 | 无（只能在画布上右键，且条目很少） | 分组菜单，可扩展 | Insert / Remove | 🟠 明显 |
| 重命名 | 不能（只能去 Inspector 改 `TrackName`） | 双击就地重命名 | 双击 + **新建后自动进入重命名** | 🟠 明显 |
| 静音 | 点色条切 `bEnabled`（无撤销） | Mute/Solo 三态图标 + 横拖批量刷 + Undo | — | 🟠 明显 |
| 行 hover / 选中反馈 | 无（只有片段 hover） | 行 hover 淡入按钮、选中分层着色 | 行 hover | 🟠 明显 |
| 吸附开关 | 永远开，关不掉 | 工具栏 Snap 总开关 + Options 下拉（帧/标记/片段…） | 外挂 `OnSnapPosition` 回调 | 🟡 一般 |
| 播放范围条 | 无（只有资产 `Length`） | `STimeRange` + `STimeRangeSlider` 可拖两端 | 同款 | 🟡 一般 |
| 空状态 / 教学 | 无 | 空区可接 Drop | 拖动时提示"按住 SHIFT 自动 scrub" | 🟡 一般 |

结论：**骨架（分栏 / 滚动 / 轨道行）必须先动**，否则后面所有交互都挂在错误的结构上。

---

## 2. UE 官方怎么做（可直接抄的部分）

### 2.1 Sequencer：重、全、可扩展

- **布局**：`SSequencer.cpp:768` 一个 `SSplitter` 两槽；左 Outliner `.Resizable(true).SizeRule(SizeToContent)`，宽度 `OnSlotResized` 写回 `USequencerSettings`（`SSequencer.cpp:782-792`），默认 250 / 最小 150（`SSequencer.h:147-148`）。
- **滚动**：**不是两个 ScrollBox**。全窗只建一根 `SScrollBar`（`SSequencer.cpp:310-314`），用 `.ExternalScrollbar()` 给树视图（`SOutlinerView.cpp:110`）；每行在 Tick 里上报自身几何（`SOutlinerViewRow.cpp:110-113` → `FCachedGeometry`），右侧 `STrackAreaView::OnArrangeChildren`（`STrackAreaView.cpp:191-237`）直接拿这个几何摆 lane，**左右天然一行对一行**。右侧滚轮/右键拖动把滚动转发给左侧（`STrackAreaView.cpp:498-538`）。
- **行是"列"拼的，不是硬编码**：`SequencerModule.cpp:509-531` 注册 Pin/Lock/Deactivate/Mute/Solo/Label/Edit/Add/KeyFrame/Nav/ColorPicker 各列；新增一个按钮不动行代码。
- **hover 淡入的五档透明度**（`SColumnToggleWidget.cpp:103-150`）：自身激活 1.0 → 鼠标在按钮上 0.8 → 父级隐式 0.35 → **仅 hover 在本行 0.25（预览）** → 无关 0.1（近乎隐形）。解决"常显太吵、常隐找不到"。
- **三态图标**（`:152-173`）：Active / Inactive / `CheckBoxIndeterminate`（父子半选）。
- **横拖批量刷**：`FColumnToggleDragDropOp`（`SColumnToggleWidget.cpp:16-61`）——按住横拖连续刷一排 Mute，整段**只开一个 `FScopedTransaction`**；选中多行时一起改（`SMuteColumnWidget.cpp:48-71`）。
- **拖放插入线按缩进偏移**：`SOutlinerViewRow.cpp:130-149`（`OffsetX = IndentLevel * 10`）；展开的行不允许 `BelowItem`（`:158-164`）。
- **刷新分两级**（`SOutlinerView.cpp:866-948`）：层级变了才重建列表，只改状态就 `RequestListRefresh()`，避免闪烁。
- **Mute/Solo 是资产内的 editor-only 装饰对象**（`TrackModel.cpp:950-981`，`UMovieSceneMuteSoloDecoration`），因此可 Undo。
- **双击 Section 兜底 = `ZoomToFit()`**（`SSequencerSection.cpp:2094-2128`）——双击永远有反馈。
- **Trim 手柄支持被遮挡的重叠片段**（`SSequencerSection.cpp:1215-1227` 遍历 `UnderlappingSegments`）。
- **拖放时 Shift = 对齐到播放头**（`SSequencerTrackAreaView.cpp:148-152`）。

### 2.2 Persona Notify 轨：轻、朴素，更贴近我们

- `SAnimTimeline.cpp:124-138`：**一根 `SScrollBar` + `SAnimTrackArea` + `SAnimOutliner` + 同一个 `ExternalScrollbar`**，一百来行搭完骨架；`:336-353` 用 `SSplitter` + 两个 `SSpacer` + Overlay 分栏。
- 固定行高 `NotificationTrackHeight = 24.f`（`AnimTimelineTrack_NotifiesPanel.cpp:20`），**没有虚拟几何、没有动态行高**——左栏直接 `SVerticalBox` 堆 `SBox.HeightOverride(24)`（`:56-118`）。
- **新建后自动进入重命名**：`PendingRenameTrackIndex` + `RegisterActiveTimer` → `EnterEditingMode()`（`AnimTimelineTrack_NotifiesPanel.cpp:110-130`）。
- **吸附外挂成一个回调**：`.OnSnapPosition(&GetModel().Get(), &FAnimModel::Snap)`（`:291`），比 Sequencer 一堆 Snap 开关简单得多。
- **Shift + 拖动 = 播放头跟随**，并顺手弹一条教学提示"Hold SHIFT while dragging … to auto scrub"（`:307-340`）——**首次操作即教学**，成本极低、体验收益极高。
- 整组拖动记录 `NodeTimeOffsets` 保持相对间距，吸附时自动补偿 `TriggerTimeOffset`（`SAnimNotifyPanel.cpp:597-777`）。

### 2.3 我们该抄谁

**骨架抄 Persona（轻），细节抄 Sequencer（精）**：
我们的轨道是**扁平数组**（`UAKAbilityAsset::Tracks`，没有父子层级），上 `STreeView` + MVVM 是杀鸡用牛刀；但"hover 淡入按钮 / 三态 / 拖放插入线 / 刷新分级 / 双击兜底"这些细节必须借，因为它们正是"操作手感"的来源。

---

## 3. 目标形态

```
┌─ SSplitter（宽可拖，存 UUAKSettings::OutlinerWidth）────────────────────────┐
│ 左：SUAKTrackOutliner                      │ 右：SUAKTimeline（只画右侧）    │
│ ┌────────────────────────────┐             │ ┌ 标尺 + 播放范围条 ─────────┐ │
│ │ ▾ 🎬 Main        2  [M S 🔒 ✕]│             │ │ 0.0   0.3   0.6   0.9    │ │
│ │ ▾ 🔥 Effect      3  [M S 🔒 ✕]│             │ ├───────────────────────────┤ │
│ │ ▾ ✨ Cue         1  [M S 🔒 ✕]│             │ │ ▓▓▓▓▓░░░░░░               │ │
│ │ ▾ 🎯 Query       1  [M S 🔒 ✕]│             │ │      ▓▓▓▓                 │ │
│ │ ▸ 🏃 Motion      0  [M S 🔒 ✕]│             │ │              ▓▓▓▓▓▓      │ │
│ └────────────────────────────┘             │ │                           │ │
│  选中行 ▸ 高亮；hover 行 ▸ 右侧按钮淡入        │ │ 空轨道：虚线 + "放个任务" │ │
└────────────────────────────────────────────┴─┴─────────────────────────────┘
        共用一个 ScrollOffset（一根 SScrollBar，右侧滚轮转发给左栏）
```

---

## 4. 结构改造（代码层面）

| 现状 | 改成 |
|---|---|
| `SUAKTimeline` 自绘左栏（140px 色条+名称）+ 右侧 | 拆成两个控件：`SUAKTrackOutliner`（左）、`SUAKTimeline`（右，去掉左栏绘制与命中） |
| 行无偏移，画不下就截断 | 共享 `ScrollOffset`（像素）+ 一根 `SScrollBar`；左右都减这个偏移绘制/命中 |
| 宽度 140px 常量 | `SSplitter`，宽度存 `UUAKSettings::OutlinerWidth`（默认 220，最小 120） |
| 排序靠按钮 | `FAKTrackDragDropOp`（`FDragDropOperation` 派生）拖行重排，带插入线 |

关键实现点：
- **滚动同步**：不做两个 ScrollBox 互相同步（会错帧）。学 Sequencer/Persona：一个 `ScrollOffset`，左栏用 `SScrollBar` 的 `ExternalScrollbar` 或直接把偏移喂给两侧；右侧 `OnMouseWheel` 无缩放修饰键时转发成滚动（Sequencer 同款）。
- **行几何**：`RowHeight` 保持常量（Persona 24 / 我们 30），轨道总高 = `Tracks.Num() * RowHeight`，超出视口即可滚。
- **刷新分级**：轨道增删/排序变化才重建行列表；Mute/Solo/Lock 变化只重绘，避免闪烁（Sequencer `SOutlinerView.cpp:866-948` 的做法）。

---

## 5. 轨道行长什么样

从左到右：

1. **折叠箭头**：已有 `UAKAbilityTrack::bCollapsed`，折叠时右侧任务行画成合并条（现在字段闲置，正好用上）。
2. **类型图标 + 色条**：复用现有 `ColorForTrack(EAKTrackType)`，加类型图标（Main / Effect / Cue / Query / Motion / Rule / Custom）。
3. **名称**：`SInlineEditableTextBlock`，**双击就地重命名**（写回 `TrackName`）；新建轨道后自动进入编辑态（抄 Persona）。
4. **状态徽标**：复合模式（Seq / Par / ×N）与任务数，扫一眼就知道这条轨道在干嘛。
5. **右侧按钮组（hover 淡入）**：
   - **Mute** = `!bEnabled`（现有字段）
   - **Lock** = 禁止编辑（新增，防误改）

   > **为什么没有 Solo（独奏）**：Sequencer 的 Solo 是从多轨媒体（音轨、摄像机轨）来的——
   > 那些轨道天然可以单独听/看。技能不是混音台，一段技能里所有轨道是**协同生效**的，
   > "只跑这一条"没有对应的运行语义（执行器也不会实现它），硬搬过来只是一个
   > 看着有用、实际什么都不做的装饰字段。要看单条轨道的效果，**把别的轨道静音**就够了。
   > （这条是被指出后删掉的：一度照抄加了 `bSolo`，运行时根本不读它。）
   - **删除** = 删这条轨道
   图标透明度按 Sequencer 五档来：激活 1.0 / 悬停按钮 0.8 / 隐式 0.35 / 行 hover 0.25 / 无关 0.1。
6. **行背景**：奇偶行 + hover + 选中（选中行整行高亮，不只是片段描边）。

**右键菜单（轨道行上）**：重命名 / 复制轨道 / 删除轨道 / 上移 / 下移 / 静音 / 锁定 / 折叠 / 新建轨道 —— 按"能否执行"分组（Sequencer `FolderModel.cpp:298-330` 的做法）。

---

## 6. 交互矩阵（补齐后）

**轨道行**
| 操作 | 行为 |
|---|---|
| 单击 | 选中轨道（Inspector 切到轨道） |
| 双击名称 | 就地重命名 |
| 拖拽行 | 重排，插入线指明落点 |
| 横拖 Mute | 按住色条左右拖，连续刷一排，一个事务 |
| 右键 | 分组菜单 |
| 滚轮 / 上下键 | 滚动 / 换选中行 |
| `F2` / `Delete` | 重命名 / 删除轨道 |

**时间轴侧（配套）**
| 操作 | 行为 |
|---|---|
| 工具栏 Snap 开关 | 可关吸附；展开可选"吸附到帧 / 到标记" |
| `Shift` + 拖动片段 | 播放头跟随（并在状态栏提示一次，抄 Persona） |
| 双击片段 | 无专门处理时兜底 `ZoomToFit`（抄 Sequencer） |
| Trim 手柄 | 重叠片段也能选中下面那条 |
| 空轨道 | 虚线 + "把任务拖到这里" |
| 播放范围条 | 标尺上可拖的两端条，直接改资产 `Length` |

---

## 7. 数据层需要动的

- `UAKAbilityTrack`：只新增 `bLocked` 一个 `UPROPERTY`（编辑器语义：防误改，运行时忽略）。
  静音继续用现有 `bEnabled`。**没有 `bSolo`**——见上一节"为什么没有 Solo"。
- `UUAKSettings`：新增 `OutlinerWidth`（float，默认 220）、`bSnapEnabled`（默认 true）、`bSnapToMarkers`（默认 true）。
- 不改轨道层级模型（保持扁平数组），不做轨道分组。

---

## 8. 分阶段（每阶段都能单独验收）

- **P0 骨架** ✅ 已落地：拆出 `SUAKTrackOutliner` + `SSplitter` + 共享滚动 + 行渲染（图标/名/徽标/选中/hover）+ 垂直滚动。**解决"轨道多了看不见"和"左栏不能调宽"这两个硬伤。**
- **P1 轨道操作** ✅ 已落地：拖行重排（带插入线）+ 右键菜单 + 双击 / 新建即重命名 + Mute / Lock 按钮 + 横拖批量刷 + 折叠（汇总条）。**"轨道操作太烂"到这一步才算真解决。**
- **P2 打磨** ✅ 已落地（除最后两项）：
  - 工具条 **吸附** 开关（关掉即自由拖动；"按帧对齐"与"对齐到标记"分开，都存进 `UUAKSettings`）
  - **Shift + 拖动片段 = 播放头跟着走**（Persona：`SAnimTimeline.cpp:307-340`），并在第一次拖动时右下角提示一次，3 秒淡出
  - **双击空白处 = 兜底 `ZoomToFit()`**（Sequencer：`SSequencerSection.cpp:2094-2128`），双击永远不会"没反应"
  - **重叠片段可 trim**：命中优先返回"最上层"，已选中的那条再优先（Sequencer 遍历 `UnderlappingSegments` 的简化版）
  - **空状态提示**：一条轨道都没有时左栏给句人话（还能右键新建）；空轨道在时间轴上标"空轨道 · 从右侧任务库添加任务"
  - **左栏键盘操作**：上下键换行（自动滚进视野）、`F2` 改名、`Delete` 删轨道
  - 未做：**播放范围条**（标尺上可拖两端改 `Length`）、**时间轴 ↔ 预览视口联动**（预览没有 seek 接口，要改 `UUAKPreviewRunner`）。

---

## 8.1 落地记录（给后来人避坑）

- **滚动同步**：左栏 `SScrollBox` 的 `OnUserScrolled` → 共享 `ScrollOffset` → 时间轴绘制/命中都减这个偏移。
  时间轴裸滚轮不再缩放，而是 `OnRequestScrollBy` 转发给左栏（Sequencer 同款：`Ctrl+滚轮` 才缩放，`Shift+滚轮` 横向平移）。
- **`SScrollBox` 是多槽容器**：内容要写 `+ SScrollBox::Slot()[...]`，不是 `ChildSlot[...]`。
- **`SBox::WidthOverride` 要 `FOptionalSize`**：左栏宽度 getter 返回 `FOptionalSize`（照 `SSequencer.h:735` 的 `GetOutlinerWidth`）。
- **行的 `SharedThis`**：`SUAKTrackOutliner::Construct` 里拿不到 `SharedThis(this)`，首帧用 `RegisterActiveTimer` 再建行；新建轨道后的"自动进入重命名"也是同一个机制。
- **hover 淡入的坑**：按钮指针别在 `SAssignNew` 之前按值捕获进 lambda（那时是空指针，`IsHovered()` 永远是 false）——存成成员再在 lambda 里读。
- **横拖批量刷**：`FAKTrackMuteDragDropOp` 自己持有 `FScopedTransaction`，拖拽期间一直开着，析构即提交；刷过的行只 `Invalidate(Paint)`，不重建（重建会打断拖拽）。
- **`UAKAbilityTrack` 没有 `Repeat` 枚举值**：只有 `Timeline / Sequence / Parallel`，重复是 `RepeatCount`（非 Timeline 模式才生效）。徽标里按这个规则显示。
- **行的键盘焦点**：`SWidget` 上没有 `SetCanKeyboardFocus`，要 override `SupportsKeyboardFocus()` 返回 `true`。
- **编译/链接顺序**：改完代码后如果编辑器开着，UBT 会报 `Unable to build while Live Coding is active`，
  或链接时报 DLL 被 `UnrealEditor.exe` 占用——关掉编辑器（或退出 Live Coding）再构建即可。

## 9. 取舍

- **不用 `STreeView` + MVVM**：我们没有父子层级，Sequencer 那套 MVVM（`OutlinerItemModel` / `TrackRowModel` / `SectionOutlinerModel`）在本场景下是纯开销。用自绘 + 共享偏移，与 Persona  Notify 轨同量级。
- **不做可拖行高**：Sequencer 也没有（行高由内容驱动），Persona 是常量 24。我们保持 30 常量，行高拖拽属于伪需求。
- **Lock 落资产而非内存**：照 Sequencer 的 Decoration 思路（可 Undo、随资产走）；我们直接用 UPROPERTY，代价更小。
- **不做的（明确划掉）**：
  - **Solo**：技能不是多轨媒体，"只跑这一条"没有运行语义（详见第 5 节）。
  - **可拖行高**：Sequencer 也没有（行高由内容驱动），Persona 是常量 24。
  - **轨道分组 / 层级**：我们的轨道是扁平数组，上 `STreeView` + MVVM 是杀鸡用牛刀。

### 9.1 任务换轨道（这里踩过一个坑）

任务属于哪条轨道 = 它的 Outer。换轨道要改 Outer（`Rename`），并且从原轨道数组摘掉、加进新轨道数组。

**坑**：一开始把"换轨道"写进了"移动"分支里，而移动分支前面有 `CanEditTaskTimes()`
（复合轨道 Sequence / Parallel 的任务时间由轨道算，不许手动改 `StartTime`）——
结果**技能的主轨道往往是 Sequence，于是上面的任务一个都挪不动**，换轨道被"不许改时间"连坐了。

**正确做法**：换轨道和改时间是两件事，判定顺序改为
**先换轨道 → 再看能不能改时间**。另外目标轨道的复合模式不限制（移过去由新轨道重新编排），
只有锁定的轨道才禁止挪进/挪出。

拖拽之外必须有条能看见的路：**右键「移到轨道 ▸」**列出所有轨道（当前轨道不列、锁定的置灰）。
（拖着改时间的时候没人会想到"再上下挪一下"。）
