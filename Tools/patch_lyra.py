#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
对拷进本工程的 Lyra 文件打补丁 —— 这就是「source 适配层」。

原则：
    Lyra 有的文件 → **copy（以 Lyra 为基线）→ patch（把我们的加回去）**
    Lyra 没有的文件 → 保持我们自己的

所有改动都声明在 Tools/lyra_patches.json 里，本文件只是执行器：
    - 幂等：已打过的自动跳过，重复跑不会叠加
    - 可查：--check 只列出待打 / 已打，不动文件
    - 可撤：--revert 按 .orig 还原

用法：
    python Tools/patch_lyra.py
    python Tools/patch_lyra.py --check
    python Tools/patch_lyra.py --revert
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parent
SPEC_FILE = TOOLS_DIR / "lyra_patches.json"
PATCHED_LIST = TOOLS_DIR / ".patched_files.txt"

MARKER = "UAK-PATCH"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig", errors="ignore")


def write(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")


def backup(path: Path) -> None:
    orig = path.with_name(path.name + ".orig")
    if not orig.is_file():
        shutil.copy2(path, orig)
        record(path)


def record(path: Path) -> None:
    """记住打过补丁的文件，供 --revert 使用"""
    rel = path.relative_to(REPO_ROOT).as_posix()
    known = set()
    if PATCHED_LIST.is_file():
        known = set(PATCHED_LIST.read_text(encoding="utf-8").splitlines())
    known.add(rel)
    PATCHED_LIST.write_text("\n".join(sorted(x for x in known if x)) + "\n", encoding="utf-8")


def patch_source(spec: dict, check_only: bool) -> int:
    pending = 0
    for item in spec.get("source", []):
        target = REPO_ROOT / item["file"]
        if not target.is_file():
            print(f"[patch] 跳过（文件不存在）：{item['file']}  —— 先跑 import_lyra")
            continue

        text = read(target)
        already = all(r["to"] in text for r in item.get("replace", []))

        if already:
            print(f"[patch] 已打：{item['id']}")
            continue

        pending += 1
        if check_only:
            print(f"[patch] 待打：{item['id']} — {item['desc']}")
            continue

        backup(target)
        text = read(target)
        for r in item.get("replace", []):
            if r["from"] in text:
                text = text.replace(r["from"], r["to"], 1)
            else:
                print(f"[patch] 警告：{item['file']} 里没找到 `{r['from']}`，跳过这条", file=sys.stderr)

        include = item.get("insert_include")
        if include and f'#include "{include}"' not in text:
            lines = text.splitlines()
            at = next((i for i, line in enumerate(lines) if line.strip().startswith("#include")), None)
            if at is not None:
                lines.insert(at + 1, f'#include "{include}"   // {MARKER}: Lyra 继承 UAK')
                text = "\n".join(lines)

        write(target, text)
        print(f"[patch] {item['id']}: {item['desc']}")
    return pending


def patch_uproject(spec: dict, check_only: bool) -> int:
    cfg = spec.get("uproject", {})
    if not cfg:
        return 0

    ours = sorted(REPO_ROOT.glob("*.uproject"))
    if not ours:
        print("[patch] 本工程下找不到 .uproject（先跑 import_lyra 拷 Lyra 的过来）", file=sys.stderr)
        return 0
    target = ours[0]

    data = json.loads(read(target))

    have_modules = {m.get("Name") for m in data.get("Modules", []) if isinstance(m, dict)}
    have_plugins = {p.get("Name") for p in data.get("Plugins", []) if isinstance(p, dict)}

    add_modules = [m for m in cfg.get("modules", []) if m not in have_modules]
    add_plugins = [p for p in cfg.get("plugins", []) if p not in have_plugins]

    if not add_modules and not add_plugins:
        print("[patch] 已打：uproject（我们的模块与插件都在）")
        return 0

    print(f"[patch] uproject 待补：模块 {add_modules} 插件 {add_plugins}")
    if check_only:
        return len(add_modules) + len(add_plugins)

    backup(target)
    data.setdefault("Modules", []).extend({"Name": m, "Type": "Runtime", "LoadingPhase": "Default"} for m in add_modules)
    data.setdefault("Plugins", []).extend({"Name": p, "Enabled": True} for p in add_plugins)
    write(target, json.dumps(data, indent="\t", ensure_ascii=False) + "\n")
    return len(add_modules) + len(add_plugins)


def patch_targets(spec: dict, check_only: bool) -> int:
    cfg = spec.get("targets", {})
    add_modules = cfg.get("add_modules", [])
    if not add_modules:
        return 0

    pending = 0
    for target in sorted((REPO_ROOT / "Source").glob("*.Target.cs")):
        text = read(target)
        missing = [m for m in add_modules if f'"{m}"' not in text]
        if not missing:
            continue

        pending += 1
        if check_only:
            print(f"[patch] 待打：{target.name} 补 {missing}")
            continue

        backup(target)
        lines = text.splitlines()
        out = []
        for line in lines:
            out.append(line)
            if "ExtraModuleNames.Add" in line:
                for module in missing:
                    out.append(line.replace('"LyraGame"', f'"{module}"').replace('"LyraEditor"', f'"{module}"'))
        write(target, "\n".join(out) + "\n")
        print(f"[patch] {target.name} 补 {missing}")
    return pending


def patch_config(spec: dict, check_only: bool) -> int:
    cfg = spec.get("config", {})
    appends = cfg.get("append", {})
    if not appends:
        return 0

    pending = 0
    for ini_name, append_rel in appends.items():
        target = REPO_ROOT / "Config" / ini_name
        source = REPO_ROOT / append_rel
        if not target.is_file():
            print(f"[patch] 跳过（ini 不存在）：Config/{ini_name}")
            continue
        if not source.is_file():
            continue

        text = read(target)
        if MARKER in text:
            print(f"[patch] 已打：Config/{ini_name}")
            continue

        pending += 1
        if check_only:
            print(f"[patch] 待打：Config/{ini_name} 追加我们的段落")
            continue

        backup(target)
        block = read(source).rstrip()
        write(target, text.rstrip() + f"\n\n; ---- {MARKER} ----\n" + block + "\n")
    return pending


def revert() -> int:
    if not PATCHED_LIST.is_file():
        print("[patch] 没有打过补丁，无需还原")
        return 0

    count = 0
    for rel in PATCHED_LIST.read_text(encoding="utf-8").splitlines():
        if not rel.strip():
            continue
        target = REPO_ROOT / rel.strip()
        orig = target.with_name(target.name + ".orig")
        if orig.is_file():
            shutil.copy2(orig, target)
            print(f"[patch] 已还原 {rel}")
            count += 1
    print(f"[patch] 还原 {count} 个文件（.orig 保留，可自行删除）")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="对拷进来的 Lyra 文件打补丁（copy → patch）")
    ap.add_argument("--check", action="store_true", help="只看状态，不改文件")
    ap.add_argument("--revert", action="store_true")
    args = ap.parse_args()

    if args.revert:
        return revert()

    if not SPEC_FILE.is_file():
        print(f"[patch] 缺少补丁清单：{SPEC_FILE}", file=sys.stderr)
        return 2

    spec = json.loads(read(SPEC_FILE))

    pending = 0
    pending += patch_source(spec, args.check)
    pending += patch_uproject(spec, args.check)
    pending += patch_targets(spec, args.check)
    pending += patch_config(spec, args.check)

    if args.check:
        print(f"[patch] 检查完成：{pending} 处待打（未改动任何文件）")
    else:
        print("[patch] 完成。继承链：UGameplayAbility → UAKGameplayAbility → ULyraGameplayAbility")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
