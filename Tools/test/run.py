"""一键测试：本地编译 → 跑三个 stage → 生成报告 / 截图 / 录屏 → 可选提交。

原实现是 `run_tests.ps1` / `run_tests.sh` 两份；现在逻辑只在这里，
shell 入口（Tools/run_tests.ps1 / run_tests.sh）只转发。

测试**只在本地跑**（有 GPU、有 UE）。CI 不跑 UE，只把本脚本产出的
docs/reports 与 docs/showcase.md 渲染到 GitHub Pages。

三个 stage：
  1. editor  编辑器内 Automation（UAK.*）：单元 / 实例 / 编辑器 UI，逐帧截图
  2. game    独立进程 -game：真实游戏世界里的技能表现，逐帧录屏
  3. net     DS + N 个客户端：各端导出 GAS 快照，比对属性是否收敛

stage 2 / 3 不用 Automation 用例，而用 Runtime 模块的 `UAK.Auto.*` 控制台命令
——因为测试模块是 Editor 类型，在 -game / -server 进程里不一定加载。
"""

from __future__ import annotations

import argparse
import shutil
import sys
import time
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # Tools/
import common as C  # noqa: E402

REPORT_PY = Path(__file__).resolve().parent / "report" / "generate_report.py"
STAGES = ("all", "editor", "game", "net")


def _window_args(headless: bool, w: int = 1280, h: int = 720) -> list[str]:
    """真实 RHI 才有像素；headless（-NullRHI）下不出素材。"""
    return ["-NullRHI"] if headless else ["-Windowed", f"-ResX={w}", f"-ResY={h}"]


def stage_editor(editor: Path, uproject: Path, report_dir: Path, log: Path,
                 args: argparse.Namespace) -> None:
    argv = [
        str(editor), str(uproject),
        "-ExecCmds=Automation RunTests " + args.filter + "; Quit",
        "-TestExit=Automation Test Queue Empty",
        f"-ReportExportPath={report_dir}",
        "-unattended", "-nopause", "-nosplash",
        f"-abslog={log}",
    ]
    argv += ["-NullRHI"] if args.headless else [
        "-Windowed", "-ResX=1280", "-ResY=720", "-UAKCaptureMedia=1",
    ]
    C.run(argv, timeout=args.timeout, what="Stage Editor")


def stage_game(editor: Path, uproject: Path, log: Path, args: argparse.Namespace) -> None:
    cmds = []
    if args.map:
        cmds.append(f"open {args.map}")
    # 周期快照：进程被脚本结束时，最后一份文件就是结束时刻的状态
    cmds += ["UAK.Auto.Snapshot Client0 2", "UAK.Auto.Record Game.Playback 300 30"]
    if args.ability_class:
        cmds.append(f"UAK.Auto.ActivateAbility {args.ability_class} 3")

    argv = [
        str(editor), str(uproject), "-game",
        "-ExecCmds=" + "; ".join(cmds),
        "-unattended", "-nopause", "-nosplash",
        f"-abslog={log}",
    ]
    argv += _window_args(args.headless)
    # 不给 Quit：跑满 --game-seconds 秒后由脚本收尾
    C.run(argv, timeout=args.game_seconds, what="Stage Game")


def stage_net(editor: Path, uproject: Path, log_dir: Path, stamp: str,
              args: argparse.Namespace) -> list[Path]:
    logs: list[Path] = []
    server_log = log_dir / f"Server-{stamp}.log"
    server = C.spawn([
        str(editor), str(uproject), "-server", f"-Port={args.port}",
        "-ExecCmds=UAK.Auto.Snapshot Server 2",
        "-unattended", "-nopause", "-nosplash", "-NullRHI",
        f"-abslog={server_log}",
    ])
    C.info(f"启动：DS（端口 {args.port}）")
    logs.append(server_log)
    time.sleep(args.server_boot_wait)

    clients = []
    for i in range(1, args.clients + 1):
        client_log = log_dir / f"Client{i}-{stamp}.log"
        cmds = [
            f"open 127.0.0.1:{args.port}",
            f"UAK.Auto.Snapshot Client{i} 2",
            f"UAK.Auto.Record Net.Client{i} 300 30",
        ]
        if args.ability_class:
            cmds.append(f"UAK.Auto.ActivateAbility {args.ability_class} 3")
        argv = [
            str(editor), str(uproject), "-game",
            "-ExecCmds=" + "; ".join(cmds),
            "-unattended", "-nopause", "-nosplash",
            f"-abslog={client_log}",
        ]
        argv += _window_args(args.headless, 640, 360)
        C.info(f"启动：客户端 {i}")
        clients.append(C.spawn(argv))
        logs.append(client_log)
        time.sleep(3)  # 错开登录，避免同时抢登录位

    C.info(f"让多端跑 {args.net_seconds} 秒…")
    time.sleep(args.net_seconds)

    for c in clients:
        C.terminate(c)
    if server.poll() is None:
        time.sleep(3)  # 等一个快照周期，确保最后一份 Server.json 是最新状态
        C.terminate(server)
    C.info("Stage Net 结束")
    return logs


def main(argv_: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="run_tests",
        description="本地一键测试：编译 → 三个 stage → 生成报告 / 素材 → 可选提交。",
    )
    ap.add_argument("--project", default=None, help="工程根目录（默认：仓库根）")
    ap.add_argument("--engine-root", "-e", default=None)
    ap.add_argument("--stage", choices=STAGES, default="all")
    ap.add_argument("--headless", action="store_true", help="-NullRHI：没 GPU 时用，不产素材")
    ap.add_argument("--skip-build", action="store_true")
    ap.add_argument("--filter", default="UAK")
    ap.add_argument("--clients", type=int, default=2)
    ap.add_argument("--port", type=int, default=7777)
    ap.add_argument("--map", default="", help="留空用工程的 GameDefaultMap")
    ap.add_argument("--ability-class", default="")
    ap.add_argument("--game-seconds", type=int, default=12)
    ap.add_argument("--net-seconds", type=int, default=25)
    ap.add_argument("--server-boot-wait", type=int, default=10)
    ap.add_argument("--timeout", type=int, default=2400, help="stage editor 的超时秒数")
    ap.add_argument("--commit", action="store_true", help="跑完把结论提交（CI 负责发布）")
    ap.add_argument("--push", action="store_true", help="提交后顺带 push")
    args = ap.parse_args(argv_)

    project_root = Path(args.project).resolve() if args.project else C.repo_root()
    uproject = C.find_uproject(project_root)
    engine_root = C.resolve_engine_root(uproject, args.engine_root)
    paths = C.engine_paths(engine_root)
    editor = paths["editor_cmd"]

    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    base_dir = project_root / "Saved" / "Automation"
    report_dir = base_dir / "Reports" / stamp
    log_dir = base_dir / "Logs"
    (report_dir / "Editor").mkdir(parents=True, exist_ok=True)
    log_dir.mkdir(parents=True, exist_ok=True)

    report_dirs: list[Path] = []
    log_files: list[Path] = []

    C.info(f"工程：{project_root}")
    C.info(f"引擎：{engine_root}")
    C.info(f"stage：{args.stage}　（headless={args.headless}）")
    if not args.headless and not shutil.which("ffmpeg"):
        C.warn("没找到 ffmpeg：帧序列不会合成 mp4（截图仍会产出）")

    # ── 编译 ──────────────────────────────────────────────────────────────
    if not args.skip_build:
        sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "build"))
        import build as build_tool  # noqa: PLC0415

        build_tool.run_ubt(project_root, engine_root, C.editor_target(uproject),
                           "Win64", "Development", timeout=args.timeout)

    # ── Stage 1：编辑器内 Automation ──────────────────────────────────────
    if args.stage in ("all", "editor"):
        stage_report = report_dir / "Editor"
        stage_log = log_dir / f"Editor-{stamp}.log"
        stage_editor(editor, uproject, stage_report, stage_log, args)
        report_dirs.append(stage_report)
        log_files.append(stage_log)

    # ── Stage 2：独立进程（-game）里的真实游戏世界 ────────────────────────
    if args.stage in ("all", "game"):
        stage_log = log_dir / f"Game-{stamp}.log"
        stage_game(editor, uproject, stage_log, args)
        log_files.append(stage_log)

    # ── Stage 3：DS + N 个客户端 ──────────────────────────────────────────
    if args.stage in ("all", "net"):
        log_files += stage_net(editor, uproject, log_dir, stamp, args)

    # ── 汇总报告 ──────────────────────────────────────────────────────────
    argv = [C.python_exe(), str(REPORT_PY),
            "--project", str(project_root),
            "--out", "docs/reports",
            "--stamp", datetime.now().strftime("%Y-%m-%d %H:%M:%S")]
    for d in report_dirs:
        argv += ["--report-dir", str(d)]
    for l in log_files:
        argv += ["--log-file", str(l)]
    report_exit = C.run(argv, what="生成报告")

    C.info(f"报告输出：{project_root / 'docs' / 'reports'}")

    # ── 可选：把结论提交回仓库（CI 只负责渲染 Pages）──────────────────────
    if args.commit:
        C.git(project_root, "add", "docs/reports", "docs/showcase.md")
        if C.git(project_root, "diff", "--staged", "--quiet") != 0:
            C.git(project_root, "commit", "-m", f"chore: update automation report ({stamp})")
            C.info("已提交测试结论")
            if args.push:
                C.git(project_root, "push")
        else:
            C.info("结论没有变化，跳过提交")

    return report_exit


if __name__ == "__main__":
    raise SystemExit(main())
