"""一键教程流水线：录制编辑器操作 → 产出截图 / 录屏 → 生成教程文档。

原实现是 `generate_tutorials.ps1` / `.sh` 两份；现在逻辑只在这里，
shell 入口（Tools/generate_tutorials.ps1 / .sh）只转发。

教程不是手写的。每一步的讲解与"要做什么"写在 Tutorials/*.json 里，
由编辑器的 `UAK.Tutorial.RunAll` 真实执行一遍并采集素材，
再由同目录的 generate_tutorials.py 渲染成 docs/tutorials/*.md。

只在本地跑（要有 UE、要有真实 RHI 才有像素），CI 只负责把文档渲染到 Pages。
用 UnrealEditor.exe（完整编辑器）而不是 -Cmd：教程要截的是资产编辑器面板。
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # Tools/
import common as C  # noqa: E402

GENERATOR = Path(__file__).resolve().parent / "generate_tutorials.py"


def generate(project_root: Path) -> None:
    code = C.run([C.python_exe(), str(GENERATOR), "--project", str(project_root)],
                 what="生成教程文档")
    if code != 0:
        C.fail(f"生成教程文档失败（exit={code}）")


def main(argv_: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="generate_tutorials",
        description="录制编辑器教程并生成文档（docs/tutorials）。",
    )
    ap.add_argument("--project", default=None, help="工程根目录（默认：仓库根）")
    ap.add_argument("--engine-root", "-e", default=None)
    ap.add_argument("--docs-only", action="store_true", help="不启动 UE，只用已有产物重新生成")
    ap.add_argument("--no-capture", action="store_true", help="跑步骤但不截图 / 不录屏")
    ap.add_argument("--tutorial", default="", help="只跑某一条（教程 id）；留空跑全部")
    ap.add_argument("--timeout", type=int, default=1200)
    ap.add_argument("--commit", action="store_true", help="生成后把结论提交")
    args = ap.parse_args(argv_)

    project_root = Path(args.project).resolve() if args.project else C.repo_root()
    script_dir = project_root / "Tutorials"
    if not script_dir.is_dir():
        C.fail(f"找不到教程脚本目录：{script_dir}")

    if args.docs_only:
        generate(project_root)
        return 0

    uproject = C.find_uproject(project_root)
    engine_root = C.resolve_engine_root(uproject, args.engine_root)
    editor = C.engine_paths(engine_root)["editor"]

    cmds = []
    if args.no_capture:
        cmds.append("UAK.Tutorial.Capture 0")
    cmds.append("UAK.Tutorial.RunAll -Quit" if not args.tutorial
                else f"UAK.Tutorial.Run {args.tutorial}")

    argv = [
        str(editor), str(uproject),
        "-ExecCmds=" + "; ".join(cmds),
        "-unattended", "-nosplash", "-stdout", "-FullStdOutLogOutput",
    ]
    code = C.run(argv, timeout=args.timeout, what="录制教程")
    if code == -1:
        C.warn("录制超时：已产出的步骤仍会写进 run.json")

    generate(project_root)

    if args.commit:
        C.git(project_root, "add", "docs/tutorials", "Tutorials", "mkdocs.yml")
        C.git(project_root, "commit", "-m", "docs(tutorials): 重新录制并生成编辑器教程")
        C.info("已提交；CI 会把它渲染到 Pages")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
