"""把下载好的 Lyra 拷进本工程并打补丁 —— 本工程由此变成"改造过后的 Lyra"。

历史上是 `import_lyra.ps1` / `import_lyra.sh` 两份 shell；现在逻辑只在这里（Python），
入口统一成 `Tools/lyra.ps1 import` / `Tools/lyra.sh import`（只转发参数）。

流程（每一步都可重跑，结果都一样）：
  0.  校验 Lyra 路径与引擎版本；把本机引擎 GUID 写进"本机覆盖"
  1.  预清理（clean.py）—— 保证工程里没有上一次的手改
  2.  拷源码：Source/LyraGame（整拷）+ Source/LyraEditor
  3.  拷外围：.uproject / Config / Target.cs（以 Lyra 为基线）
  4.  拷资产：Plugins/GameFeatures/*、Lyra 自带插件、Content/（约 3 GB，不进 git）
  5.  打补丁：ULyraGameplayAbility 继承 UAKGameplayAbility 等（patch.py）
  6.  记账（.lyra_paths.txt）+ 快照（verify.py --update-state）

铁律：**不改 Lyra 的内容**。必须改的地方全部写在 lyra_patches.json 里。
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # Tools/
import common as C  # noqa: E402

LYRA_DIR = Path(__file__).resolve().parent                  # Tools/lyra/
PATHS_FILE = LYRA_DIR / ".lyra_paths.txt"                   # clean.py 照单全删
MANIFEST = LYRA_DIR / "lyra_manifest.txt"                   # 给人看的来源清单
LOCAL_SPEC = LYRA_DIR / "lyra_patches.local.json"           # 本机覆盖（不进仓库）
PATCH_PY = LYRA_DIR / "patch.py"
VERIFY_PY = LYRA_DIR / "verify.py"

# Content/ 下这几个是我们自己的，永不覆盖
OUR_CONTENT = {"UAK", "Script", "Collections", "Developers", "README.md"}


class Recorder:
    """拷贝时记账：导入了什么（clean 才知道该删什么）+ 来源清单（给人看）。"""

    def __init__(self) -> None:
        self.imported: list[str] = []
        self.manifest: list[str] = []

    def copy(self, root: Path, src: Path, rel: str) -> None:
        dest = root / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        if src.is_dir():
            if dest.exists():
                shutil.rmtree(dest)
            shutil.copytree(src, dest)
        else:
            shutil.copy2(src, dest)
        self.imported.append(rel)


def _find_lyra_uproject(lyra: Path) -> Path:
    for name in ("LyraStarterGame.uproject", "Lyra.uproject"):
        candidate = lyra / name
        if candidate.is_file():
            return candidate
    found = sorted(lyra.glob("*.uproject"))
    if not found:
        C.fail(f"{lyra} 下找不到 .uproject")
    return found[0]


def _write_engine_association(engine_root: Path) -> None:
    """本机引擎 GUID → lyra_patches.local.json（绝不进仓库）。"""
    guid = os.environ.get("UE_ENGINE_GUID") or C.engine_guid_for(engine_root)
    if not guid:
        C.warn("没查到 --engine-root 对应的引擎 GUID，跳过引擎关联补丁"
               "（可设环境变量 UE_ENGINE_GUID）")
        return
    LOCAL_SPEC.write_text(
        "{\n"
        '\t"comment": "由 lyra import 自动生成：本机引擎的 GUID。不进仓库。",\n'
        f'\t"uproject": {{ "set_engine_association": "{guid}" }}\n'
        "}\n",
        encoding="utf-8",
    )
    C.info(f"[import] 引擎关联：{guid} ← {engine_root}（写进 lyra_patches.local.json，不进仓库）")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="lyra import",
        description="把 Lyra 拷进本工程并打补丁（先清 → 拷 → 打补丁 → 校验）。",
    )
    ap.add_argument("-l", "--lyra-path", required=True, help="下载好的 Lyra 工程目录")
    ap.add_argument("-e", "--engine-root", default=None,
                    help="引擎目录（默认取 UE_ENGINE_ROOT，或按 .uproject 的 GUID 反查）")
    ap.add_argument("--skip-assets", action="store_true", help="不拷资产（等价于 --keep-assets）")
    ap.add_argument("--skip-patch", action="store_true", help="拷完不打补丁")
    ap.add_argument("--keep-assets", action="store_true", help="预清理时保留上次拷来的资产")
    ap.add_argument("--no-clean", action="store_true", help="跳过预清理（不推荐，只调试用）")
    ap.add_argument("--force", action="store_true", help=argparse.SUPPRESS)  # 兼容旧用法
    args = ap.parse_args(argv)

    root = C.repo_root()
    rec = Recorder()

    # ── 0. 校验 ───────────────────────────────────────────────────────────
    lyra = Path(args.lyra_path).expanduser().resolve()
    if not lyra.is_dir():
        C.fail(f"Lyra 目录不存在：{lyra}")
    lyra_uproject = _find_lyra_uproject(lyra)
    try:
        assoc = json.loads(lyra_uproject.read_text(encoding="utf-8-sig")).get("EngineAssociation", "")
    except json.JSONDecodeError:
        assoc = ""
    C.info(f"[import] Lyra：{lyra_uproject}（引擎 {assoc}）")
    if isinstance(assoc, str) and assoc and not assoc.startswith(("5.4", "5.6", "5.8")):
        C.warn(f"UAK 验证过 5.4 / 5.6 / 5.8，当前版本 {assoc} 建议先确认")

    engine_root: Path | None = None
    if args.engine_root and Path(args.engine_root).is_dir():
        engine_root = Path(args.engine_root).resolve()
        _write_engine_association(engine_root)

    # ── 1. 预清理 ─────────────────────────────────────────────────────────
    keep_assets = args.keep_assets or args.skip_assets
    if args.skip_assets:
        C.info("[import] --skip-assets：资产不重拷，沿用现有的（等价于 --keep-assets）")

    # 清单里原有的资产条目要继续带着：这次不拷，但它们还在磁盘上
    prev_assets: list[str] = []
    if PATHS_FILE.is_file():
        prev_assets = [ln.strip() for ln in PATHS_FILE.read_text(encoding="utf-8").splitlines()
                       if ln.strip().startswith(("Plugins/", "Content/"))]

    if args.no_clean:
        C.warn("[import] --no-clean：跳过预清理，工程里可能残留上次的手改（只用于调试）")
    else:
        C.info("[import] 预清理（保证这次导入是唯一的变量来源）…")
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import clean  # noqa: PLC0415

        clean.main(["--keep-assets"] if keep_assets else [])

    # ── 2. 拷源码 ─────────────────────────────────────────────────────────
    # LyraGame 各子目录互相引用，按白名单挑目录拷会编不过，所以整拷。
    lyra_game = lyra / "Source" / "LyraGame"
    if lyra_game.is_dir():
        for f in sorted(lyra_game.iterdir()):
            if f.is_file():
                rec.copy(root, f, f"Source/LyraGame/{f.name}")
            elif f.is_dir():
                rec.copy(root, f, f"Source/LyraGame/{f.name}")
                rec.manifest.append(f"Source/LyraGame/{f.name}")
                C.step(f"[import] 源码：{f.name}")
    lyra_editor = lyra / "Source" / "LyraEditor"
    if lyra_editor.is_dir():
        rec.copy(root, lyra_editor, "Source/LyraEditor")
        rec.manifest.append("Source/LyraEditor")
        C.step("[import] 源码：LyraEditor")

    # ── 3. 拷外围（Lyra 是基线，我们的东西靠后面的 patch 加回去）─────────
    C.info("[import] 拷外围文件（以 Lyra 为基线）…")
    rec.copy(root, lyra_uproject, lyra_uproject.name)
    C.step(f"[import] .uproject：{lyra_uproject.name} ← Lyra")

    lyra_config = lyra / "Config"
    if lyra_config.is_dir():
        for ini in sorted(lyra_config.glob("*.ini")):
            rec.copy(root, ini, f"Config/{ini.name}")
    C.step("[import] Config ← Lyra")

    lyra_source = lyra / "Source"
    if lyra_source.is_dir():
        for t in sorted(lyra_source.glob("*.Target.cs")):
            rec.copy(root, t, f"Source/{t.name}")
    C.step("[import] Target.cs ← Lyra")

    # ── 4. 拷资产（大，不进 git）──────────────────────────────────────────
    if not args.skip_assets:
        C.info("[import] 拷资产（Content 约 1.8 GB + 插件约 1.4 GB，不进 git）…")

        gf_src = lyra / "Plugins" / "GameFeatures"
        if gf_src.is_dir():
            for gf in sorted(p for p in gf_src.iterdir() if p.is_dir()):
                rec.copy(root, gf, f"Plugins/GameFeatures/{gf.name}")
                C.step(f"[import] 资产插件：GameFeatures/{gf.name}")

        lyra_plugins = lyra / "Plugins"
        engine_plugins = engine_root / "Engine" / "Plugins" if engine_root else None
        if lyra_plugins.is_dir():
            for p in sorted(x for x in lyra_plugins.iterdir() if x.is_dir()):
                if p.name == "GameFeatures":
                    continue
                # 引擎里已有的同名插件跳过，避免重名冲突；没给引擎目录就全拷
                if engine_plugins and engine_plugins.is_dir() and (engine_plugins / p.name).is_dir():
                    C.step(f"[import] 引擎自带，跳过：{p.name}")
                    continue
                rec.copy(root, p, f"Plugins/{p.name}")
                C.step(f"[import] Lyra 插件：{p.name}")
            if not engine_plugins:
                C.warn("[import] 未指定 --engine-root：Lyra 自带插件不做筛选，全部拷进来"
                       "（可能与引擎自带插件重名）")

        content_src = lyra / "Content"
        if content_src.is_dir():
            for item in sorted(content_src.iterdir()):
                if item.name in OUR_CONTENT:
                    continue
                rec.copy(root, item, f"Content/{item.name}")
                C.step(f"[import] /Game 内容：Content/{item.name}")
        else:
            C.warn(f"[import] {lyra} 下没有 Content/：工程缺 /Game 根资产，启动必崩")

    platforms = lyra / "Platforms"
    if platforms.is_dir():
        rec.copy(root, platforms, "Platforms")
        C.step("[import] 平台配置：Platforms/")

    # ── 5. 打补丁（唯一允许改动 Lyra 的地方）──────────────────────────────
    if not args.skip_patch:
        code = C.run([C.python_exe(), str(PATCH_PY)], what="打补丁")
        if code != 0:
            C.fail("打补丁失败")

    # ── 6. 记账 + 快照 ────────────────────────────────────────────────────
    imported = sorted(set(rec.imported) | set(prev_assets))
    PATHS_FILE.write_text("\n".join(imported) + "\n", encoding="utf-8")
    C.info(f"[import] 导入清单（clean 按它删）：{PATHS_FILE}")

    MANIFEST.write_text("\n".join(sorted(set(rec.manifest))) + "\n", encoding="utf-8")
    C.info(f"[import] 来源清单：{MANIFEST}")

    # ── 7. 校验：确认工程 = Lyra 原版 + 声明过的补丁 ──────────────────────
    code = C.run([C.python_exe(), str(VERIFY_PY), "-l", str(lyra), "--update-state"],
                 what="导入后校验")
    if code != 0:
        C.warn("[import] 校验没通过：上面列出的文件与 Lyra 原版不一致")

    print()
    C.info("[import] 完成。下一步：")
    print("  1) 编译：Tools/build.ps1（或 UnrealBuildTool 编 LyraGameEditor）")
    print("  2) 打开工程，确认 ULyraGameplayAbility 已经能用 UAK 时间轴")
    print("  3) 把 Lyra 的 BP 技能按 UAK 资产重写（见 docs/examples/lyra-integration.md）")
    print()
    C.info("[import] 随时回到干净状态：Tools/lyra.ps1 clean（然后重跑 import 即可复现）")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
