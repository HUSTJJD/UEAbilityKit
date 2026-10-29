"""Tools 的公共基础：路径、引擎定位、子进程、日志。

设计前提（见 Tools/README.md）：**shell 脚本只是薄入口**，真正的逻辑都在 Python 里。
所以这里提供的是"任何功能都会用到的那几件事"，避免每个子目录各写一份：

- 工程根 / 工程文件定位（不假设当前工作目录）
- 引擎根目录定位（环境变量 → .uproject 的 EngineAssociation → 注册表）
- 起子进程（可超时强杀）、跑 git

本机信息（绝对路径 / 引擎 GUID）永远不写进仓库，只从命令行、环境变量或注册表来。
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

# ── 路径 ──────────────────────────────────────────────────────────────────
# Tools/<功能>/<脚本>.py  →  Tools 在 parents[1]，仓库根在 parents[2]
_THIS = Path(__file__).resolve()
TOOLS_DIR = _THIS.parent
REPO_ROOT = TOOLS_DIR.parent


def repo_root() -> Path:
    """仓库根（= UE 工程根）。"""
    return REPO_ROOT


def tools_dir() -> Path:
    return TOOLS_DIR


# ── 日志 ──────────────────────────────────────────────────────────────────
def info(msg: str) -> None:
    print(f"[UAK] {msg}", flush=True)


def step(msg: str) -> None:
    print(f"  · {msg}", flush=True)


def warn(msg: str) -> None:
    print(f"[UAK] 警告：{msg}", file=sys.stderr, flush=True)


def fail(msg: str, code: int = 1) -> "NoReturn":  # type: ignore[valid-type]
    print(f"[UAK] 错误：{msg}", file=sys.stderr, flush=True)
    raise SystemExit(code)


# ── 工程文件 ──────────────────────────────────────────────────────────────
def find_uproject(project_root: Path) -> Path:
    """定位 .uproject：工程根下应当只有一份，直接取它。"""
    found = sorted(project_root.glob("*.uproject"))
    if not found:
        fail(f"{project_root} 下没有 .uproject，请先创建示例工程")
    return found[0]


def editor_target(uproject: Path) -> str:
    """编译目标跟着工程走：<工程名> → <工程名>Editor。"""
    return uproject.stem + "Editor"


# ── 引擎定位 ──────────────────────────────────────────────────────────────
REG_KEY = r"SOFTWARE\Epic Games\Unreal Engine\Builds"


def _registry_engine(guid: str) -> str | None:
    """Windows：按 .uproject 里的 EngineAssociation GUID 反查引擎目录。"""
    if sys.platform != "win32":
        return None
    try:
        import winreg

        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, REG_KEY) as key:
            value, _ = winreg.QueryValueEx(key, guid)
            return str(value)
    except OSError:
        return None


def engine_guid_for(engine_root: Path) -> str | None:
    """反查：给定引擎目录，找它在注册表里登记的 GUID（写本机覆盖用）。"""
    if sys.platform != "win32":
        return None
    try:
        import winreg

        want = str(engine_root).replace("/", "\\").rstrip("\\").lower()
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, REG_KEY) as key:
            i = 0
            while True:
                try:
                    name, value, _ = winreg.EnumValue(key, i)
                except OSError:
                    break
                i += 1
                if not name.startswith("{"):
                    continue
                if str(value).replace("/", "\\").rstrip("\\").lower() == want:
                    return name
    except OSError:
        return None
    return None


def resolve_engine_root(uproject: Path, engine_root: str | None = None) -> Path:
    """引擎目录：显式指定 > 环境变量 UE_ENGINE_ROOT > 注册表（按 GUID 反查）。"""
    candidates = [engine_root, os.environ.get("UE_ENGINE_ROOT", "")]
    for c in candidates:
        if c and Path(c).is_dir():
            return Path(c).resolve()

    try:
        assoc = json.loads(uproject.read_text(encoding="utf-8-sig")).get("EngineAssociation", "")
    except (OSError, json.JSONDecodeError):
        assoc = ""
    if isinstance(assoc, str) and assoc.startswith("{") and assoc.endswith("}"):
        found = _registry_engine(assoc)
        if found and Path(found).is_dir():
            return Path(found).resolve()

    fail("找不到 UE 引擎：用 --engine-root 指定，或设置环境变量 UE_ENGINE_ROOT")


def engine_paths(engine_root: Path) -> dict[str, Path]:
    """引擎里的几个固定位置；缺一个就报错，别等到 UBT 里才炸。"""
    paths = {
        "ubt": engine_root / "Engine" / "Binaries" / "DotNET" / "UnrealBuildTool" / "UnrealBuildTool.dll",
        "editor_cmd": engine_root / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe",
        "editor": engine_root / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe",
        "source": engine_root / "Engine" / "Source",
    }
    for name, path in paths.items():
        if not path.exists():
            fail(f"引擎目录不完整，缺少 {name}：{path}")
    return paths


# ── 子进程 ────────────────────────────────────────────────────────────────
def run(argv: list[str], cwd: Path | None = None, timeout: int | None = None,
        env: dict[str, str] | None = None, what: str = "") -> int:
    """起一个进程；超时就杀。返回退出码（被杀返回 -1）。"""
    label = what or " ".join(str(a) for a in argv[:2])
    info(f"启动：{label}")
    proc = subprocess.Popen(argv, cwd=str(cwd) if cwd else None,
                            env={**os.environ, **(env or {})} if env else None)
    try:
        proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        warn(f"{label} 达到 {timeout}s，结束进程")
        proc.kill()
        try:
            proc.wait(timeout=15)
        except subprocess.TimeoutExpired:
            pass
        return -1
    info(f"{label} 退出码：{proc.returncode}")
    return proc.returncode


def spawn(argv: list[str], cwd: Path | None = None,
          env: dict[str, str] | None = None) -> subprocess.Popen:
    """后台起进程（多端测试用：DS 与客户端要并存）。"""
    return subprocess.Popen(argv, cwd=str(cwd) if cwd else None,
                            env={**os.environ, **(env or {})} if env else None)


def terminate(proc: subprocess.Popen, wait: int = 15) -> None:
    if proc.poll() is None:
        proc.kill()
        try:
            proc.wait(timeout=wait)
        except subprocess.TimeoutExpired:
            pass


def python_exe() -> str:
    """用当前解释器跑子脚本，避免 'python 不在 PATH' 这类环境差异。"""
    return sys.executable


def git(repo: Path, *args: str, check: bool = False) -> int:
    code = subprocess.run(["git", *args], cwd=str(repo)).returncode
    if check and code != 0:
        fail(f"git {' '.join(args)} 失败（exit={code}）")
    return code
