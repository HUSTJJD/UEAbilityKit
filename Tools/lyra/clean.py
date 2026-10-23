"""清除本工程里所有从 Lyra 拷进来的内容，回到"还没导入 Lyra"的干净状态。

历史上是 `clean_lyra.ps1` / `clean_lyra.sh` 两份 shell；现在逻辑只在这里（Python），
入口统一成 `Tools/lyra.ps1 clean` / `Tools/lyra.sh clean`（只转发参数）。

为什么必须先清：拷进来的 Lyra 一旦被人手改过，编译/测试就会"因为改过而通过"，
把真正的 bug 藏起来。工程应当永远等于：

    Lyra 原版 + Tools/lyra/lyra_patches.json 里声明过的补丁
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # Tools/
import common as C  # noqa: E402

STATE_DIR = Path(__file__).resolve().parent           # Tools/lyra/
PATHS_FILE = STATE_DIR / ".lyra_paths.txt"            # 上次导入的清单
MANIFEST = STATE_DIR / "lyra_manifest.txt"            # 给人看的来源清单

FIXED_DIRS = [
    "Source/LyraGame",
    "Source/LyraEditor",
    "Config",
    "Platforms",
    "Content/ShooterCore",
    "Content/ShooterMaps",
]
JUNK_FILES = ["*.orig", "*.uak.bak"]
STATE_FILES = [
    "Tools/lyra/.patched_files.txt",
    "Tools/lyra/.lyra_state.json",
    "Tools/lyra/.lyra_paths.txt",
    "Tools/lyra/lyra_manifest.txt",
]
DEEP_DIRS = ["Binaries", "Intermediate", "DerivedDataCache", "Saved"]


def _remove(root: Path, rel: str, dry_run: bool, removed: list[int]) -> None:
    full = (root / rel).resolve()
    if not str(full).startswith(str(root) + "\\") and not str(full).startswith(str(root) + "/"):
        C.fail(f"[clean] 拒绝删除仓库之外的路径：{full}")
    if not full.exists():
        return
    if dry_run:
        C.info(f"[clean] 将删除：{rel}")
        removed[0] += 1
        return
    if full.is_dir():
        import shutil

        shutil.rmtree(full)
    else:
        full.unlink()
    C.step(f"[clean] 删除：{rel}")
    removed[0] += 1


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="lyra clean",
        description="清除从 Lyra 拷进来的源码 / 配置 / 资产，回到未导入状态。",
    )
    ap.add_argument("--keep-assets", action="store_true",
                    help="保留从 Lyra 拷来的资产（约 1.4 GB），只清源码与配置（迭代快）")
    ap.add_argument("--deep", action="store_true",
                    help="连 Binaries/Intermediate/DerivedDataCache/Saved 一起清")
    ap.add_argument("--dry-run", action="store_true", help="只列出来，不真删")
    ap.add_argument("--keep-plugin", action="append", default=None,
                    help="额外的保留插件（可重复）；默认保留 UEAbilityKit 与 UnLuaNova")
    args = ap.parse_args(argv)

    root = C.repo_root()
    keep_plugins = args.keep_plugin if args.keep_plugin else ["UEAbilityKit", "UnLuaNova"]
    removed = [0]

    C.info(f"[clean] 工程根目录：{root}")

    # 1. 上次导入的清单（最准；--keep-assets 时 Plugins/ 与 Content/ 要留着）
    if PATHS_FILE.is_file():
        for line in PATHS_FILE.read_text(encoding="utf-8").splitlines():
            rel = line.strip()
            if not rel:
                continue
            if args.keep_assets and (rel.startswith("Plugins/") or rel.startswith("Content/")):
                continue
            _remove(root, rel, args.dry_run, removed)

    # 2. 固定清单（清单丢了也能清干净）
    for rel in FIXED_DIRS:
        _remove(root, rel, args.dry_run, removed)

    source_dir = root / "Source"
    if source_dir.is_dir():
        for f in sorted(source_dir.glob("*.Target.cs")):
            _remove(root, f"Source/{f.name}", args.dry_run, removed)
    for f in sorted(root.glob("*.uproject")):  # 根目录的 .uproject 整份都是 Lyra 的
        _remove(root, f.name, args.dry_run, removed)

    # 3. 资产：Plugins/ 下除了我们自己的插件，其余都是 Lyra 拷来的
    plugin_root = root / "Plugins"
    if args.keep_assets:
        C.info("[clean] --keep-assets：保留资产插件（注意：资产不会重新校验，只是省时间）")
    elif plugin_root.is_dir():
        for p in sorted(plugin_root.iterdir()):
            if not p.is_dir():
                continue
            if p.name in keep_plugins:
                C.step(f"[clean] 保留（我们的）：Plugins/{p.name}")
                continue
            _remove(root, f"Plugins/{p.name}", args.dry_run, removed)

    # 4. 补丁痕迹
    for pattern in JUNK_FILES:
        for f in root.rglob(pattern):
            if ".git" in f.parts or "Plugins/UnLuaNova" in f.as_posix():
                continue
            _remove(root, f.relative_to(root).as_posix(), args.dry_run, removed)
    for rel in STATE_FILES:
        _remove(root, rel, args.dry_run, removed)

    # 5. 引擎生成物（可选）
    if args.deep:
        for rel in DEEP_DIRS:
            _remove(root, rel, args.dry_run, removed)
        C.info("[clean] --deep：已清引擎生成物（下次编译会是全量，慢但最干净）")

    print()
    if args.dry_run:
        C.info(f"[clean] 预演完成：共 {removed[0]} 项将被删除（未改动任何文件）")
    else:
        C.info(f"[clean] 完成：删除 {removed[0]} 项，工程已回到未导入 Lyra 的状态")
        C.info("[clean] 下一步：Tools/lyra.ps1 import --lyra-path <你的 Lyra 目录>")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
