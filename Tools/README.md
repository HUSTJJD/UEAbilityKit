# Tools

本仓库**唯一的脚本目录**，按功能分子目录。

两条原则：

1. **shell 只是入口**：根目录的 `.ps1` / `.sh` 是薄壳，只负责把参数转发给 Python，
   一行逻辑都不写。所有功能都在 Python 里（跨平台、可测试、能复用）
2. **一个功能一个入口**：入口只做转发，用子命令区分（`setup` / `doctor` 之类），
   不重复造平行的 `Scripts/` 目录

> 不要新建 `Scripts/` 之类的平行目录，也不要往 shell 里加逻辑 ——
> 需要新功能就在对应子目录里写 Python，需要新入口就在根目录加一个同样的薄壳。

## 入口（根目录，薄壳）

| 入口 | 转发到 | 作用 |
|---|---|---|
| `build.ps1` / `build.sh` | `build/build.py` | 命令行编译（绕开 Build.bat 的锁） |
| `run_tests.ps1` / `run_tests.sh` | `test/run.py` | 一键测试：三 stage + 报告 + 素材 |
| `generate_tutorials.ps1` / `.sh` | `tutorial/run.py` | 教程录制 + 文档生成 |
| `unluanova.ps1` / `.sh` | `unluanova/cli.py` | 脚本层：`setup` 一次写对 / `doctor` 只读体检（子命令） |

```powershell
./Tools/build.ps1
./Tools/run_tests.ps1 --stage editor --filter UAK.Unit
./Tools/generate_tutorials.ps1 --docs-only
./Tools/unluanova.ps1 setup
./Tools/unluanova.ps1 doctor --strict     # 警告也算失败（新机器接入用）
```

```bash
./Tools/run_tests.sh --headless          # Linux / macOS 同上
```

## 功能目录

| 目录 | 内容 |
|---|---|
| `build/` | `build.py`：直接调 UnrealBuildTool |
| `test/` | `run.py`（三 stage 编排）+ `report/generate_report.py`（报告 / Showcase 生成器） |
| `tutorial/` | `run.py`（录制编排）+ `generate_tutorials.py`（教程页渲染器） |
| `hygiene/` | `check_no_local_paths.py`：检查仓库里有没有混入本机路径 / 引擎 GUID |
| `common.py` | 共用基础：工程根与 `.uproject` 定位、引擎目录定位（环境变量 → GUID 反查）、子进程（可超时强杀）、git |

## 约定

- **工程根 = 脚本目录的上一级**：Python 里从 `common.py` 拿 `repo_root()`，
  shell 里用 `$MyInvocation.MyCommand.Path` / `BASH_SOURCE`；不写绝对路径、不假设当前目录
- **`.ps1` 保留 UTF-8 BOM**：Windows PowerShell 5.1 没有 BOM 就按系统 ANSI（GBK）读脚本，
  中文会乱码并在解析阶段报错（PowerShell 7 读 UTF-8，看不出问题）。
  根目录的薄壳因此**只用 ASCII 注释**，从根上避开这个坑
- **本机信息不进仓库**：引擎目录走 `--engine-root` / `UE_ENGINE_ROOT`，引擎 GUID 从注册表反查，
  都不落盘到仓库里
- **运行期状态不进仓库**：测试报告 / 素材之外的中间产物一律 gitignore
