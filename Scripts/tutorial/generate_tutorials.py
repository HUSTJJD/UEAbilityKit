#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
UAK 教程文档生成器：把"教程脚本 + 录制产物"变成可以发布的教学文档。

输入
  1) 教程脚本：<Project>/Tutorials/*.json
     每一步同时是三样东西：讲解（say / do / expect）、要执行的动作（action / args）、
     要采集的素材（capture）。三者同源，所以文档永远不会和真实编辑器对不上。
  2) 录制产物：<Project>/Saved/UAKAutomation/Tutorials/<tutorial>/run.json（UAK.Tutorial.Run 产出）
     + 同目录下的 step_XX_*.png 与 step_XX_*/frames/*.png
  3) 日志标记（可选）：UAK_TUTORIAL_RUN <id> <OK|FAIL> steps=n failed=n path=...

输出（提交回仓库，CI 只负责渲染）
  docs/tutorials/<id>.md           每篇教程一页：分步讲解 + 截图 + 录屏 + 自动化细节
  docs/tutorials/index.md          教程总览（含每篇的状态与步骤数）
  docs/tutorials/media/<id>/**     截图与 mp4（由帧序列合成，需要 ffmpeg）
  docs/tutorials/manifest.json     机读清单（CI / 徽章用）

只依赖标准库；ffmpeg 可选（没有 ffmpeg 时不合成 mp4，但截图与文本照常产出）。

用法
  python Scripts/tutorial/generate_tutorials.py --project .                # 有录制产物时用产物
  python Scripts/tutorial/generate_tutorials.py --project . --offline      # 只看脚本，先出文档骨架
  python Scripts/tutorial/generate_tutorials.py --project . --no-mkdocs    # 不同步 mkdocs.yml
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path

NAV_BEGIN = "      # UAK_TUTORIALS_BEGIN"
NAV_END = "      # UAK_TUTORIALS_END"

STATUS_ICON = {"OK": "✅", "FAIL": "❌", "SKIP": "⏭️", "ABORT": "⏹️", "NORUN": "⚪"}
LEVEL_LABEL = {"beginner": "入门", "intermediate": "进阶", "advanced": "高阶"}


# ── 读取 ──────────────────────────────────────────────────────────────────
def load_json(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8-sig", errors="ignore"))
    except Exception:
        return None


def load_scripts(script_dir: Path) -> list[dict]:
    if not script_dir.is_dir():
        return []
    scripts = []
    for path in sorted(script_dir.glob("*.json")):
        data = load_json(path)
        if isinstance(data, dict) and data.get("steps"):
            data["_path"] = path
            scripts.append(data)
    return scripts


def load_run(media_root: Path, tutorial_id: str) -> dict | None:
    data = load_json(media_root / tutorial_id / "run.json")
    return data if isinstance(data, dict) else None


# ── 素材 ──────────────────────────────────────────────────────────────────
def collect_media(media_root: Path, tutorial_id: str, out_dir: Path, fps: int) -> dict:
    """把截图拷进 docs，把帧序列合成 mp4。返回 {step_id: {...}}。"""
    found: dict[str, dict] = {}
    src_root = media_root / tutorial_id
    if not src_root.is_dir():
        return found

    dst_root = out_dir / "media" / tutorial_id
    dst_root.mkdir(parents=True, exist_ok=True)

    # 截图：step_01_xxx.png
    for png in sorted(src_root.rglob("step_*.png")):
        rel = png.relative_to(src_root)
        dst = dst_root / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(png, dst)
        step_id = rel.stem.split("_", 2)[2] if rel.stem.count("_") >= 2 else rel.stem
        found.setdefault(step_id, {})["shot"] = f"media/{tutorial_id}/{rel.as_posix()}"

    # 帧序列：step_01_xxx/frames/*.png → mp4
    for frames_dir in sorted(src_root.rglob("frames")):
        if not frames_dir.is_dir() or not any(frames_dir.glob("*.png")):
            continue
        step_dir = frames_dir.parent
        rel_step = step_dir.relative_to(src_root)
        step_id = rel_step.name.split("_", 2)[2] if rel_step.name.count("_") >= 2 else rel_step.name

        dst_frames = dst_root / rel_step / "frames"
        dst_frames.mkdir(parents=True, exist_ok=True)
        for png in sorted(frames_dir.glob("*.png")):
            shutil.copy2(png, dst_frames / png.name)

        mp4_rel = f"media/{tutorial_id}/{rel_step.as_posix()}/step.mp4"
        if shutil.which("ffmpeg"):
            cmd = [
                "ffmpeg", "-y", "-loglevel", "error",
                "-framerate", str(fps),
                "-i", str(dst_frames / "frame_%05d.png"),
                "-c:v", "libx264", "-pix_fmt", "yuv420p",
                str(dst_root / rel_step / "step.mp4"),
            ]
            try:
                subprocess.run(cmd, check=True, capture_output=True)
                found.setdefault(step_id, {})["video"] = mp4_rel
            except Exception:
                pass
        found.setdefault(step_id, {})["frames"] = len(list(dst_frames.glob("*.png")))

    # 血缘树 / 导出 Json 等文本素材
    for txt in sorted(src_root.rglob("export/*.json")):
        rel = txt.relative_to(src_root)
        dst = dst_root / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(txt, dst)

    return found


# ── 渲染 ──────────────────────────────────────────────────────────────────
def advance_text(adv: dict) -> str:
    """把引导模式的完成条件说成人话，写进折叠块里。"""
    if not adv:
        return ""
    conds = []
    if adv.get("minTracks") is not None:
        conds.append(f"轨道数 ≥ {adv['minTracks']}")
    if adv.get("minTasks") is not None:
        conds.append(f"任务数 ≥ {adv['minTasks']}")
    if adv.get("minCursorTime") is not None:
        conds.append(f"光标 ≥ {adv['minCursorTime']}s")
    if adv.get("requireSelectedTask"):
        conds.append("已选中一个任务")
    if adv.get("requireSelectedTrack"):
        conds.append("已选中一条轨道")
    if adv.get("expectErrors") is not None:
        conds.append(f"校验错误数 == {adv['expectErrors']}")
    if not conds:
        return ""
    extra = f"；超时 {adv['timeout']}s 自动放行" if adv.get("timeout") else ""
    return "；".join(conds) + extra


def build_step_md(index: int, step: dict, result: dict | None, media: dict, sid: str) -> list[str]:
    step_id = step.get("id", f"step{index:02d}")
    title = step.get("title") or step_id
    res = result or {}
    status = res.get("status", "NORUN")
    media_for_step = media.get(step_id, {})

    # { #step-N }：attr_list 显式指定锚点，目录里的链接才不会因中英文混排失效
    lines = [f"## 第 {index} 步 · {title} {{: #step-{index} }}", ""]

    if step.get("say"):
        lines += [f"**为什么这么做**：{step['say']}", ""]

    if step.get("do"):
        lines += [f"**操作**：{step['do']}", ""]

    if step.get("expect"):
        lines += [f"**应该看到**：{step['expect']}", ""]

    shot = media_for_step.get("shot") or (res.get("shot") and f"media/{sid}/{res['shot']}")
    if shot:
        lines += [f"![{title}]({shot})", ""]

    video = media_for_step.get("video")
    if video:
        lines += [
            f'<video src="{video}" controls muted playsinline '
            f'style="width:100%;max-width:960px;border-radius:8px"></video>',
            "",
        ]
    elif media_for_step.get("frames"):
        lines += [f"> 已采集 {media_for_step['frames']} 帧，本机缺 ffmpeg，未合成 mp4。", ""]

    # 自动化细节：教程与脚本同源的证明，也是排障入口
    detail = [
        f"**动作**：`{step.get('action', 'Note')}`",
        "",
    ]
    if step.get("args"):
        detail += ["```json", json.dumps(step["args"], ensure_ascii=False, indent=2), "```", ""]

    adv = advance_text(step.get("advance") or {})
    if adv:
        detail += [f"**引导模式**：做到「{adv}」就自动进入下一步。", ""]
    else:
        detail += ["**引导模式**：这一步没有可自动判定的目标状态，由读者点「下一步」。", ""]
    if res:
        detail += [
            f"**执行结果**：{STATUS_ICON.get(status, '⚪')} {status}"
            f"　耗时 {res.get('duration', 0):.2f}s"
            f"　轨道 {res.get('tracks', 0)}"
            f"　任务 {res.get('tasks', 0)}",
            "",
        ]
        if step.get("action") == "Validate":
            detail += [f"> 校验：错误 {res.get('errors', 0)} · 警告 {res.get('warnings', 0)}", ""]
        for note in res.get("notes", []) or []:
            detail += [f"- {note}", ""]
    else:
        detail += ["> 这一步还没有录制产物（跑一次 `Scripts/generate_tutorials.ps1` 即可补上）。", ""]

    # 折叠块内的空行也要带缩进，否则 Markdown 会把块提前截断
    lines += ["??? note \"自动化是怎么做这一步的\"", ""] + \
             [("    " + l) if l else "    " for l in detail] + [""]
    return lines


def build_tutorial_md(script: dict, run: dict | None, media: dict, stamp: str) -> str:
    sid = script.get("id", "tutorial")
    steps = script.get("steps", [])
    run_steps = (run or {}).get("steps", [])
    results_by_order = {s.get("id"): r for s, r in zip(steps, run_steps)}

    total = len(steps)
    ok = sum(1 for s in steps if (results_by_order.get(s.get("id")) or {}).get("status") == "OK")
    recorded = bool(run)

    lines = [
        f"# {script.get('title', sid)}",
        "",
        script.get("summary", ""),
        "",
        f"**难度**：{LEVEL_LABEL.get(script.get('level', 'beginner'), script.get('level', '-'))}"
        f"　**预计**：{script.get('duration', '-')}"
        f"　**步骤**：{total}",
        "",
    ]

    if recorded:
        lines += [
            f"> 本页由 `Scripts/tutorial/generate_tutorials.py` 自动生成，素材来自 "
            f"`UAK.Tutorial.Run` 的真实录制（{run.get('stamp', stamp)}）。请勿手改。",
            "",
            f"录制结论：{ok}/{total} 步通过　引擎 {run.get('engine', '-')}",
            "",
        ]
    else:
        lines += [
            "!!! warning \"还没有录制产物\"",
            "    本页目前只由教程脚本渲染，截图 / 录屏尚未采集。",
            "    在本机执行 `Scripts/generate_tutorials.ps1`（需要 UE 与非 NullRHI）即可补齐素材，",
            "    文档结构与讲解不受影响。",
            "",
        ]

    if script.get("prerequisites"):
        lines += ["## 先修", ""]
        lines += [f"- {p}" for p in script["prerequisites"]] + [""]

    if script.get("checks"):
        lines += ["## 学完自查", ""]
        lines += [f"- [ ] {c}" for c in script["checks"]] + [""]

    # 目录
    lines += ["## 步骤一览", "", "| # | 步骤 | 动作 | 状态 |", "| --- | --- | --- | --- |"]
    for i, step in enumerate(steps, 1):
        res = results_by_order.get(step.get("id")) or {}
        lines.append(
            f"| {i} | [{step.get('title', step.get('id'))}](#step-{i}) "
            f"| `{step.get('action', 'Note')}` "
            f"| {STATUS_ICON.get(res.get('status', 'NORUN'), '⚪')} |"
        )
    lines.append("")

    lines += ["---", ""]

    for i, step in enumerate(steps, 1):
        lines += build_step_md(i, step, results_by_order.get(step.get("id")), media, sid)
        lines += ["---", ""]

    lines += [
        "## 这篇文档是怎么来的",
        "",
        "```text",
        "Tutorials/<id>.json      ← 人写的：讲解 + 动作 + 采集声明（唯一事实来源）",
        "   ↓ UAK.Tutorial.Run（编辑器内自动化，真实点一遍）",
        "Saved/UAKAutomation/Tutorials/<id>/{run.json, *.png, frames/}",
        "   ↓ Scripts/tutorial/generate_tutorials.py",
        "docs/tutorials/<id>.md   ← 你正在看的这页",
        "```",
        "",
        "教程不是「先写文档再补截图」，而是**录制过程即文档**：",
        "每一步的讲解与每一步的动作写在同一个对象里，改了编辑器行为就必须改脚本，",
        "否则 `Automation RunTests UAK.Tutorial` 会失败。",
        "",
        f"_生成时间：{stamp}_",
        "",
    ]
    return "\n".join(lines) + "\n"


def build_index_md(entries: list[dict], stamp: str) -> str:
    lines = [
        "# 编辑器教程",
        "",
        "这一组教程**不是手写的**：每一步的讲解、操作与截图/录屏，都由 "
        "`UAK.Tutorial.Run` 在真实编辑器里执行一遍后自动产出。",
        "",
        "```text",
        "Tutorials/*.json  →  UAK.Tutorial.Run  →  Saved/.../run.json + 截图 + 帧序列",
        "                                        →  generate_tutorials.py  →  docs/tutorials/*.md",
        "```",
        "",
        "| 教程 | 难度 | 步骤 | 最近录制 | 状态 |",
        "| --- | --- | --- | --- | --- |",
    ]

    for e in entries:
        icon = "✅ 已录制" if e["recorded"] else "⚪ 待录制"
        lines.append(
            f"| [{e['title']}]({e['id']}.md) | {LEVEL_LABEL.get(e['level'], e['level'])} | {e['steps']} | "
            f"{e['stamp']} | {icon} |"
        )

    lines += [
        "",
        "## 本地重录一遍",
        "",
        "```powershell",
        "./Scripts/generate_tutorials.ps1            # 打开编辑器录制 + 生成文档",
        "./Scripts/generate_tutorials.ps1 -DocsOnly  # 只用已有产物重新生成文档",
        "./Scripts/generate_tutorials.ps1 -NoCapture # 只验证步骤，不采素材",
        "```",
        "",
        "```bash",
        "./Scripts/generate_tutorials.sh",
        "./Scripts/generate_tutorials.sh --docs-only",
        "```",
        "",
        "完整流程见 [测试与自动化](../testing.md#教程流水线文档也是自动化的产物)。",
        "",
        f"_生成时间：{stamp}_",
        "",
    ]
    return "\n".join(lines) + "\n"


# ── mkdocs 导航同步 ───────────────────────────────────────────────────────
def sync_mkdocs(mkdocs_path: Path, entries: list[dict]) -> None:
    if not mkdocs_path.is_file():
        return

    # 保留原有换行风格：仓库里 LF / CRLF 混用，整篇重写会污染 diff
    raw = mkdocs_path.read_bytes().decode("utf-8-sig")
    newline = "\r\n" if "\r\n" in raw else "\n"
    lines = raw.replace("\r\n", "\n").split("\n")

    nav_lines = [f"      - {e['title']}: tutorials/{e['id']}.md" for e in entries]

    if NAV_BEGIN in raw and NAV_END in raw:
        begin = lines.index(NAV_BEGIN)
        end = lines.index(NAV_END)
        lines[begin + 1:end] = nav_lines
    else:
        anchor = next((i for i, l in enumerate(lines) if l.strip().startswith("- 编辑器教程:")), None)
        if anchor is not None:
            insert_at = anchor + 2
            lines[insert_at:insert_at] = [NAV_BEGIN] + nav_lines + [NAV_END]
        else:
            # 没有这个 section：插在 nav 末尾
            nav_start = next((i for i, l in enumerate(lines) if l.strip() == "nav:"), None)
            if nav_start is None:
                return
            insert_at = len(lines)
            for i in range(nav_start + 1, len(lines)):
                if lines[i].strip() and not lines[i].startswith((" ", "\t", "#")):
                    insert_at = i
                    break
            block = ["  - 编辑器教程:", "      - 教程总览: tutorials/index.md", NAV_BEGIN] \
                + nav_lines + [NAV_END]
            lines[insert_at:insert_at] = block

    # split("\n") 已经把结尾换行变成最后的空串；只有原本没有结尾换行时才需要补
    tail = "" if raw.endswith(("\n", "\r\n")) else "\n"
    mkdocs_path.write_bytes((newline.join(lines) + tail).encode("utf-8"))


# ── 主流程 ────────────────────────────────────────────────────────────────
def main() -> int:
    parser = argparse.ArgumentParser(description="生成 UAK 编辑器教程文档")
    parser.add_argument("--project", required=True, help="工程根目录")
    parser.add_argument("--script-dir", default="", help="教程脚本目录（默认 <project>/Tutorials）")
    parser.add_argument("--media-root", default="", help="录制产物目录（默认 Saved/UAKAutomation/Tutorials）")
    parser.add_argument("--out", default="docs/tutorials", help="输出目录（相对工程根）")
    parser.add_argument("--mkdocs", default="mkdocs.yml", help="mkdocs 配置（相对工程根）")
    parser.add_argument("--offline", action="store_true", help="只读脚本，不读录制产物")
    parser.add_argument("--no-mkdocs", action="store_true", help="不同步 mkdocs.yml 导航")
    args = parser.parse_args()

    root = Path(args.project).resolve()
    script_dir = Path(args.script_dir) if args.script_dir else root / "Tutorials"
    media_root = Path(args.media_root) if args.media_root \
        else root / "Saved" / "UAKAutomation" / "Tutorials"
    out_dir = root / args.out
    out_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    scripts = load_scripts(script_dir)
    if not scripts:
        print(f"[UAK] {script_dir} 下没有教程脚本", file=sys.stderr)
        return 2

    entries = []
    failed = 0

    for script in scripts:
        sid = script.get("id", script["_path"].stem)
        run = None if args.offline else load_run(media_root, sid)
        media = {} if args.offline else collect_media(
            media_root, sid, out_dir, int(script.get("fps", 30) or 30))

        (out_dir / f"{sid}.md").write_text(
            build_tutorial_md(script, run, media, stamp), encoding="utf-8")

        steps = script.get("steps", [])
        ok = sum(1 for r in (run or {}).get("steps", []) if r.get("status") == "OK")
        if run and ok != len(steps):
            failed += 1

        entries.append({
            "id": sid,
            "title": script.get("title", sid),
            "level": script.get("level", "beginner"),
            "steps": len(steps),
            "recorded": bool(run),
            "stamp": (run or {}).get("stamp", "-"),
            "passed": ok,
        })

    (out_dir / "index.md").write_text(build_index_md(entries, stamp), encoding="utf-8")
    (out_dir / "manifest.json").write_text(
        json.dumps({"stamp": stamp, "tutorials": entries}, ensure_ascii=False, indent=2),
        encoding="utf-8")

    if not args.no_mkdocs:
        sync_mkdocs(root / args.mkdocs, entries)

    recorded = sum(1 for e in entries if e["recorded"])
    print(f"[UAK] 教程文档已生成：{out_dir}")
    print(f"[UAK] 教程 {len(entries)} 篇，其中 {recorded} 篇有录制产物")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
