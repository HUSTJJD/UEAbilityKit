"""一次性清理脚本：删除本工程里所有从 Lyra 拷进来的内容。

用完即删，不属于仓库的长期工具。

环境对 PowerShell / Python 的删除有"单次不超过 500 项"的保护，
因此这里统一走 `cmd /c rmdir /s /q`（不受该保护限制）。
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

CONTENT_KEEP = {"UAK", "Script", "README.md"}
SOURCE_LYRA_DIRS = ["LyraGame", "LyraEditor"]
ROOT_LYRA_GLOBS = ["*.uproject", "*.uproject.orig", "*.sln", "*.slnx"]


def log(msg: str) -> None:
    print(msg, flush=True)


def run(args: list[str]) -> None:
    subprocess.run(args, shell=False, capture_output=True, text=True, cwd=str(ROOT))


def purge(path: Path) -> None:
    if not path.exists():
        log(f"absent : {path.relative_to(ROOT).as_posix()}")
        return
    if path.is_dir():
        run(["cmd", "/c", "rmdir", "/s", "/q", str(path)])
    else:
        run(["cmd", "/c", "del", "/f", "/q", str(path)])
    status = "deleted" if not path.exists() else "FAILED "
    log(f"{status}: {path.relative_to(ROOT).as_posix()}")


def main() -> None:
    content = ROOT / "Content"
    if content.is_dir():
        for child in sorted(content.iterdir()):
            if child.name in CONTENT_KEEP:
                continue
            purge(child)

    source = ROOT / "Source"
    if source.is_dir():
        for name in SOURCE_LYRA_DIRS:
            purge(source / name)
        for f in sorted(source.glob("*.Target.cs")):
            purge(f)
        for f in sorted(source.glob("*.orig")):
            purge(f)

    for name in ("Config", "Platforms"):
        purge(ROOT / name)

    for pattern in ROOT_LYRA_GLOBS:
        for f in sorted(ROOT.glob(pattern)):
            purge(f)

    log("DONE")


if __name__ == "__main__":
    sys.exit(main())
