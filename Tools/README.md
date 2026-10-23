# Tools

**本仓库唯一的脚本目录。** 构建、测试、教程、Lyra 导入与补丁，全部放在这里 ——
历史上曾并存一个 `Scripts/`，两者职责重叠（都是"工程自动化脚本"），已合并进本目录。

> 约定：不要再新建 `Scripts/` 之类的平行目录。新脚本一律进 `Tools/`，
> 需要分组就建子目录（现有 `patches/`、`report/`、`tutorial/`）。

## 文件地图

| 文件 | 作用 |
|---|---|
| `import_lyra.ps1` / `.sh` | **导入 Lyra**：清 → 拷 → 打补丁 → 记账 → 校验（一条命令复现工程） |
| `patch_lyra.py` | 打 / 撤销 / 检查 Lyra 补丁（幂等，改动先备份 `.orig`） |
| `clean_lyra.ps1` / `.sh` | 清除拷进来的 Lyra 与补丁痕迹（`-KeepAssets` / `-Deep` / `-DryRun`） |
| `verify_lyra.py` | 校验有没有人手改过 Lyra（与上游逐文件比 sha256） |
| `lyra_patches.json` | 补丁清单 —— **适配层的完整定义**（`.local.json` 放本机专属覆盖） |
| `lyra_manifest.txt` | 拷进来的 Lyra 文件来源与改动记录 |
| `check_no_local_paths.py` | 检查仓库里有没有混入本机路径 / 引擎 GUID |
| `build.ps1` | 命令行编译（绕过 `Build.bat` 的锁） |
| `run_tests.ps1` / `.sh` | 测试一键跑：编译 + 三个 stage + 生成报告 |
| `generate_tutorials.ps1` / `.sh` | 教程：编辑器内真实录制 + 渲染成文档 |
| `patches/config/*.append` | 打进 Lyra ini 的配置片段（键位、打包路径、Lua 后端…） |
| `report/generate_report.py` | 报告 / Showcase 生成器 |
| `tutorial/generate_tutorials.py` | 教程页渲染器 |

## 常用命令

```powershell
./Tools/import_lyra.ps1 -LyraPath <Lyra 目录> -EngineRoot <引擎目录>   # 搭工程
./Tools/build.ps1                                                      # 编译
./Tools/run_tests.ps1 -Stage Editor -Filter UAK.Unit                   # 跑测试
./Tools/generate_tutorials.ps1 -Tutorial 01-first-ability              # 重录一篇教程
python Tools/patch_lyra.py --check                                     # 看补丁状态
python Tools/verify_lyra.py                                            # 有没有人手改过 Lyra
```

## 约定

- **工程根目录 = 脚本目录的上一级**：脚本一律用 `$PSScriptRoot/..`（或 `dirname "$0"/..`）定位工程根，
  不写绝对路径、不假设当前工作目录
- **`.ps1` 保留 UTF-8 BOM**：Windows PowerShell 5.1 没有 BOM 就按系统 ANSI（GBK）读脚本，
  中文注释会被解成乱码并在解析阶段报错（PowerShell 7 读 UTF-8，看不出问题）
- **本机专属配置不进仓库**：引擎 GUID 之类写 `lyra_patches.local.json`（已 gitignore）
- **运行期状态不进仓库**：`.lyra_paths.txt`、`.lyra_state.json`、`.patched_files.txt`
