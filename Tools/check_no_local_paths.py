#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
仓库卫生检查：git 跟踪的文件里不许出现本机路径 / 引擎 GUID。

原因：这些东西一进仓库，别人 clone 下来就是坏的（打不开、编不过）。
本机相关的东西一律走"运行时生成 + 不进仓库"：
    - Lyra / 引擎目录：命令行参数（-LyraPath / -EngineRoot）或环境变量 UE_ENGINE_ROOT
    - 引擎 GUID：import_lyra 自动写进 Tools/lyra_patches.local.json（已 gitignore）
    - 导入清单与快照：Tools/.lyra_paths.txt / .lyra_state.json（已 gitignore）

用法：
    python Tools/check_no_local_paths.py            # 有问题就退出码 1
    python Tools/check_no_local_paths.py --verbose  # 把所有命中的行打出来

豁免：确需出现样例时，在行尾加 `# noqa: local-path`。
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

PATTERNS = [
    (re.compile(r"[A-Za-z]:[\\/](Users|learning|Project|Games|CodeBuddy)[\\/]"), "本机盘符路径（如 D:\\learning\\...）"),
    (re.compile(r"[\\/]Users[\\/][A-Za-z0-9._-]+[\\/]"), "macOS 用户目录（/Users/xxx/）"),
    (re.compile(r"[\\/]home[\\/][A-Za-z0-9._-]+[\\/]"), "Linux 用户目录（/home/xxx/）"),
    (re.compile(r"~[\\/](learning|Project|CodeBuddy)[\\/]"), "本机目录（~/learning/...）"),
    (re.compile(r"\{[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}\}"), "引擎 GUID（各机器不同）"),
]

SKIP_SUFFIXES = {".png", ".jpg", ".jpeg", ".gif", ".ico", ".uasset", ".umap", ".pdf", ".zip"}
# 规则自己就是这些pattern，别把自己报出来
SKIP_FILES = {"Tools/check_no_local_paths.py"}
NOQA = "noqa: local-path"
MAX_BYTES = 2 * 1024 * 1024


def tracked_files() -> list[str]:
    out = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=REPO_ROOT,
        capture_output=True,
        check=True,
    ).stdout.decode("utf-8", errors="ignore")
    return [p for p in out.split("\0") if p]


def main() -> int:
    verbose = "--verbose" in sys.argv
    hits: list[tuple[str, int, str, str]] = []

    for rel in tracked_files():
        if rel.replace("\\", "/") in SKIP_FILES:
            continue
        path = REPO_ROOT / rel
        if path.suffix.lower() in SKIP_SUFFIXES or not path.is_file():
            continue
        if path.stat().st_size > MAX_BYTES:
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for lineno, line in enumerate(text.splitlines(), 1):
            if NOQA in line:
                continue
            for pattern, why in PATTERNS:
                if pattern.search(line):
                    hits.append((rel, lineno, why, line.strip()[:120]))
                    break

    if not hits:
        print(f"[hygiene] 干净：{len(tracked_files())} 个跟踪文件里没有本机路径 / 引擎 GUID")
        return 0

    for rel, lineno, why, line in hits:
        print(f"[hygiene] {rel}:{lineno}  {why}\n    {line}")
    print(f"[hygiene] 共 {len(hits)} 处。本机相关的东西请用参数/环境变量/lyra_patches.local.json，别写进仓库", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
