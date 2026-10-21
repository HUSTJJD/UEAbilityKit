#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
校验：工程里的 Lyra 内容 ==「Lyra 原版 + lyra_patches.json 里声明过的补丁」，一处手改都不许有。

为什么要它：
    拷进来的 Lyra 文件一旦被人手改过，编译/测试就会"因为改过而通过"，把 bug 藏起来。
    唯一允许的改动写在 Tools/lyra_patches.json 里，且只由 patch_lyra.py 自动打——
    所以"重新 import 一遍就能复现"的状态才是可信的基线。

两种校验方式：
    1) 在线（权威，需要 Lyra 源目录）：把工程里每个 Lyra 文件与 Lyra 源逐字节比 sha256
       python Tools/verify_lyra.py -l <你的 Lyra 目录>
    2) 离线（快，不需要 Lyra 源）：与上次导入时记录的快照比
       python Tools/verify_lyra.py

其它：
    python Tools/verify_lyra.py -l <Lyra> --update-state   # 刷新快照（import_lyra 会自动调）
    python Tools/verify_lyra.py --self-test                # 自检（不动工程里的任何文件）

退出码：0 干净 / 1 有手改或补丁缺失 / 2 用法错误
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import tempfile
from datetime import datetime
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parent
SPEC_FILE = TOOLS_DIR / "lyra_patches.json"
STATE_FILE = TOOLS_DIR / ".lyra_state.json"

# 哪些目录算"从 Lyra 拷来的"
LYRA_TREES = ("Source/LyraGame", "Source/LyraEditor", "Platforms")
LYRA_PREFIXES = ("Source/LyraGame/", "Source/LyraEditor/", "Config/", "Platforms/", "Plugins/GameFeatures/")

OK = "ok"            # 与 Lyra 原版一致
PATCHED = "patched"  # 与 Lyra 不一致，但 lyra_patches.json 里声明过
TAMPERED = "tampered"  # 与 Lyra 不一致且没声明 —— 手改，必须清掉重来
GENERATED = "generated"  # Lyra 里没有这个文件（由补丁片段生成）
NO_STATE = "no-state"    # 离线模式下没有快照可比


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def deep_merge(base: dict, extra: dict) -> dict:
    for key, value in extra.items():
        if isinstance(value, dict) and isinstance(base.get(key), dict):
            deep_merge(base[key], value)
        else:
            base[key] = value
    return base


def load_spec(spec_file: Path) -> dict:
    if not spec_file.is_file():
        return {}
    spec = json.loads(spec_file.read_text(encoding="utf-8-sig", errors="ignore"))
    local = spec_file.with_name("lyra_patches.local.json")   # 本机覆盖，不进仓库
    if local.is_file():
        spec = deep_merge(spec, json.loads(local.read_text(encoding="utf-8-sig", errors="ignore")))
    return spec


def is_declared(rel: str, spec: dict) -> bool:
    """这个文件是不是 lyra_patches.json 声明过要改的"""
    rel = rel.replace("\\", "/")
    for item in spec.get("source", []):
        if str(item.get("file", "")).replace("\\", "/") == rel:
            return True
    if spec.get("uproject") and rel.endswith(".uproject") and "/" not in rel:
        return True  # 整个 .uproject 都是要补模块/插件的
    if spec.get("targets", {}).get("add_modules") and rel.startswith("Source/") and rel.endswith(".Target.cs") \
            and rel.count("/") == 1:
        return True
    for ini in spec.get("config", {}).get("append", {}):
        if rel == f"Config/{ini}":
            return True
    return False


def lyra_counterpart(rel: str) -> str | None:
    """工程里的相对路径 → Lyra 源里的相对路径（None = 不是从 Lyra 拷来的）"""
    for prefix in LYRA_PREFIXES:
        if rel.startswith(prefix):
            return rel
    if rel.startswith("Source/") and rel.endswith(".Target.cs") and rel.count("/") == 1:
        return rel
    if rel.endswith(".uproject") and "/" not in rel:
        return rel
    return None


def collect(root: Path, deep: bool = False) -> list[str]:
    """列出工程里所有 Lyra 来源的文件（跳过 patch 留下的 .orig / .uak.bak 备份）"""
    rels: list[str] = []
    for sub in LYRA_TREES:
        base = root / sub
        if base.is_dir():
            rels += [p.relative_to(root).as_posix() for p in base.rglob("*") if p.is_file()]
    src = root / "Source"
    if src.is_dir():
        rels += [f"Source/{p.name}" for p in src.glob("*.Target.cs")]
    rels += [p.name for p in root.glob("*.uproject")]
    cfg = root / "Config"
    if cfg.is_dir():
        rels += [f"Config/{p.name}" for p in cfg.glob("*.ini")]
    if deep:
        gf = root / "Plugins" / "GameFeatures"
        if gf.is_dir():
            rels += [p.relative_to(root).as_posix() for p in gf.rglob("*") if p.is_file()]
    return sorted(r for r in set(rels) if not r.endswith((".orig", ".uak.bak")))


def patch_evidence(rel: str, spec: dict, text: str) -> bool:
    """声明的补丁在文件里是否真的生效（只看 source 那类替换，其余由 patches_pending 兜住）"""
    for item in spec.get("source", []):
        if str(item.get("file", "")).replace("\\", "/") != rel:
            continue
        for r in item.get("replace", []):
            if r["to"] not in text:
                return False
        inc = item.get("insert_include")
        if inc and f'#include "{inc}"' not in text:
            return False
    return True


def verify_online(
    root: Path, lyra_root: Path, spec: dict, deep: bool, stored: dict | None = None, refresh: bool = False
) -> tuple[list[dict], dict]:
    """与 Lyra 源逐文件比对（有快照时顺带比对快照，能抓出"打过补丁后又手改"）"""
    stored = stored or {}
    results: list[dict] = []
    state: dict = {}
    for rel in collect(root, deep):
        cur_file = root / rel
        declared = is_declared(rel, spec)
        cur_sha = sha256(cur_file)
        src_rel = lyra_counterpart(rel)

        pristine_sha = None
        if src_rel:
            src_file = lyra_root / src_rel
            if src_file.is_file():
                pristine_sha = sha256(src_file)

        if pristine_sha is None:
            status = GENERATED if declared else TAMPERED
            detail = "Lyra 里没有这个文件" + ("" if declared else "，补丁清单也没声明")
        elif pristine_sha == cur_sha:
            status = OK
            detail = ""
        elif not declared:
            status = TAMPERED
            detail = "与 Lyra 原版不同，且补丁清单没声明 —— 本地手改"
        else:
            entry = stored.get(rel) or {}
            if not refresh and entry.get("current") and cur_sha not in (entry.get("current"), entry.get("pristine")):
                status = TAMPERED
                detail = "与导入时的快照不同 —— 打完补丁之后又被手改了"
            elif not patch_evidence(rel, spec, cur_file.read_text(encoding="utf-8", errors="ignore")):
                status = TAMPERED
                detail = "声明的补丁没生效（内容被改过）"
            else:
                status = PATCHED
                detail = "已声明的补丁"

        results.append({"rel": rel, "status": status, "detail": detail})
        state[rel] = {"pristine": pristine_sha, "current": cur_sha, "declared": declared}
    return results, state


def verify_offline(root: Path, spec: dict, state: dict, deep: bool) -> list[dict]:
    """与上次导入的快照比对（不需要 Lyra 源）"""
    results: list[dict] = []
    for rel in collect(root, deep):
        entry = state.get(rel)
        if not entry:
            results.append({"rel": rel, "status": NO_STATE, "detail": "快照里没有这个文件（先跑一次 -l 导入）"})
            continue

        cur_sha = sha256(root / rel)
        declared = bool(entry.get("declared")) or is_declared(rel, spec)

        if cur_sha == entry.get("current"):
            status = PATCHED if declared and entry.get("pristine") != cur_sha else OK
            detail = "已声明的补丁" if status == PATCHED else ""
        elif entry.get("pristine") and cur_sha == entry["pristine"]:
            status = OK if not declared else TAMPERED
            detail = "" if not declared else "补丁没打上（文件是 Lyra 原版）"
        else:
            status = TAMPERED
            detail = "与快照不同 —— 本地手改"
        results.append({"rel": rel, "status": status, "detail": detail})
    return results


def patches_pending(root: Path, spec: dict) -> int:
    """声明了但没打上的补丁数量（复用 patch_lyra 的检查逻辑）"""
    try:
        if str(TOOLS_DIR) not in sys.path:
            sys.path.insert(0, str(TOOLS_DIR))
        import patch_lyra as P  # noqa: E402
    except Exception:
        return -1

    P.REPO_ROOT = root
    P.PATCHED_LIST = root / "Tools" / ".patched_files.txt"
    pending = 0
    pending += P.patch_source(spec, check_only=True)
    pending += P.patch_uproject(spec, check_only=True)
    pending += P.patch_targets(spec, check_only=True)
    if (root / "Config").is_dir():  # 没导入 Config 就别去动它
        pending += P.patch_config(spec, check_only=True)
    return pending


def dump_state(path: Path, lyra_root: Path | None, state: dict) -> None:
    payload = {
        "generated": datetime.now().isoformat(timespec="seconds"),
        "lyra": str(lyra_root) if lyra_root else None,
        "files": state,
    }
    path.write_text(json.dumps(payload, indent=1, ensure_ascii=False, sort_keys=True), encoding="utf-8")


def report(results: list[dict], quiet: bool) -> int:
    counts: dict[str, int] = {}
    for r in results:
        counts[r["status"]] = counts.get(r["status"], 0) + 1
        if r["status"] in (TAMPERED, NO_STATE) or (not quiet and r["status"] != OK):
            mark = {"ok": "·", "patched": "P", "generated": "G", "tampered": "✗", "no-state": "?"}[r["status"]]
            print(f"[verify] {mark} {r['rel']}" + (f"  —— {r['detail']}" if r["detail"] else ""))

    print(
        "[verify] 结果："
        f"{counts.get(OK, 0)} 个与 Lyra 一致、"
        f"{counts.get(PATCHED, 0)} 个已声明补丁、"
        f"{counts.get(GENERATED, 0)} 个由补丁生成、"
        f"{counts.get(TAMPERED, 0)} 处手改、"
        f"{counts.get(NO_STATE, 0)} 个没有快照"
    )
    return counts.get(TAMPERED, 0)


def self_test() -> int:
    """在临时目录里跑一遍（不碰真实工程）"""
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "repo"
        lyra = Path(tmp) / "lyra"
        rel = "Source/LyraGame/AbilitySystem/LyraGameplayAbility.h"
        (root / Path(rel).parent).mkdir(parents=True, exist_ok=True)
        (lyra / Path(rel).parent).mkdir(parents=True, exist_ok=True)
        (root / "Tools").mkdir(parents=True, exist_ok=True)

        original = "class ULyraGameplayAbility : public UGameplayAbility\n"
        (lyra / rel).write_text(original, encoding="utf-8")

        spec = {"source": [{"id": "ability-base", "file": rel,
                            "replace": [{"from": "public UGameplayAbility", "to": "public UAKGameplayAbility"}]}]}
        spec_file = root / "Tools" / "lyra_patches.json"
        spec_file.write_text(json.dumps(spec, ensure_ascii=False), encoding="utf-8")

        # 1) 与 Lyra 一致 → 干净
        (root / rel).write_text(original, encoding="utf-8")
        r1, _ = verify_online(root, lyra, spec, deep=False)
        assert report(r1, quiet=True) == 0, "原样拷贝应判定为干净"

        # 2) 手改（补丁痕迹都没了）→ 报错
        (root / rel).write_text(original.replace("public UGameplayAbility", "public UMyHackAbility"), encoding="utf-8")
        r2, _ = verify_online(root, lyra, spec, deep=False)
        assert report(r2, quiet=True) == 1, "手改应被判定为 tampered"
        tampered = [r for r in r2 if r["status"] == TAMPERED]
        assert tampered and tampered[0]["rel"] == rel

        # 3) 声明过的补丁 → 干净
        patched_text = original.replace("public UGameplayAbility", "public UAKGameplayAbility")
        (root / rel).write_text(patched_text, encoding="utf-8")
        r3, state = verify_online(root, lyra, spec, deep=False)
        assert report(r3, quiet=True) == 0, "声明过的补丁不应报错"
        assert sum(1 for r in r3 if r["status"] == PATCHED) == 1

        # 3b) 打完补丁又手改（补丁痕迹还在）→ 靠快照抓出来
        (root / rel).write_text(patched_text + "// 我偷偷改了一行\n", encoding="utf-8")
        r3b, _ = verify_online(root, lyra, spec, deep=False, stored=state)
        assert report(r3b, quiet=True) == 1, "打完补丁后的手改也应被抓到"
        (root / rel).write_text(patched_text, encoding="utf-8")

        # 4) 离线：手改同样能被抓到
        bad = verify_offline(root, spec, state, deep=False)
        assert sum(1 for r in bad if r["status"] == PATCHED) == 1, "离线模式下打过补丁的文件应仍算干净"
        (root / rel).write_text("我又手改了一下\n", encoding="utf-8")
        bad2 = verify_offline(root, spec, state, deep=False)
        assert sum(1 for r in bad2 if r["status"] == TAMPERED) == 1, "离线模式应能发现手改"

    print("[verify] 自检通过：原样=干净、手改=报错、声明过的补丁=干净（在线/离线都成立）")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="校验工程里的 Lyra 内容没有手改")
    ap.add_argument("-l", "--lyra", default="", help="Lyra 源工程目录（给了就做权威比对）")
    ap.add_argument("--update-state", action="store_true", help="刷新 Tools/.lyra_state.json 快照")
    ap.add_argument("--deep", action="store_true", help="连 Plugins/GameFeatures 的资产一起校验（慢）")
    ap.add_argument("--quiet", action="store_true", help="只报问题")
    ap.add_argument("--self-test", action="store_true", help="在临时目录里自检，不碰真实工程")
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    lyra_root = Path(args.lyra).resolve() if args.lyra else None
    if lyra_root is not None and not lyra_root.is_dir():
        print(f"[verify] Lyra 目录不存在：{lyra_root}", file=sys.stderr)
        return 2

    spec = load_spec(SPEC_FILE)
    if not spec:
        print(f"[verify] 缺少补丁清单：{SPEC_FILE}", file=sys.stderr)
        return 2

    problems = 0

    raw_state = json.loads(STATE_FILE.read_text(encoding="utf-8")) if STATE_FILE.is_file() else {}
    stored = raw_state.get("files", {})

    if lyra_root is not None:
        print(f"[verify] 与 Lyra 源比对：{lyra_root}")
        results, state = verify_online(REPO_ROOT, lyra_root, spec, args.deep, stored=stored, refresh=args.update_state)
        if args.update_state:
            dump_state(STATE_FILE, lyra_root, state)
            print(f"[verify] 快照已写入：{STATE_FILE}")
    else:
        if not stored:
            print("[verify] 没有快照（Tools/.lyra_state.json）。先跑：python Tools/verify_lyra.py -l <Lyra 目录>", file=sys.stderr)
            return 2
        print(f"[verify] 离线比对快照（{raw_state.get('generated', '?')}，源 {raw_state.get('lyra')}）")
        results = verify_offline(REPO_ROOT, spec, stored, args.deep)

    pending = patches_pending(REPO_ROOT, spec)
    problems += report(results, args.quiet)

    if pending > 0:
        print(f"[verify] ✗ 有 {pending} 处 lyra_patches.json 声明的补丁没打上（重跑 patch_lyra.py 或 import_lyra）")
        problems += pending
    elif pending == 0:
        print("[verify] 补丁清单已全部打上")

    if problems:
        print(f"[verify] 不干净：{problems} 处。修法只有一条 —— 清掉重来：Tools/clean_lyra 然后重新 import_lyra", file=sys.stderr)
        return 1

    print("[verify] 干净：工程 = Lyra 原版 + 声明过的补丁，没有手改")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
