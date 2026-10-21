#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
UAK 自动化报告生成器。

输入（由 Scripts/run_tests.ps1 / .sh 产生）：
  1) UE 的自动化报告：<ReportDir>/index.json（或若干 Report-*.json）
     兜底：解析 Saved/Automation/Logs/Automation.log
  2) 测试产物：Saved/UAKAutomation/ 下的截图、帧序列、血缘树文本

输出：
  docs/reports/latest.md      结论（Markdown，给 GitHub 看）
  docs/reports/latest.html    结论（自包含 HTML，给 GitHub Pages 看）
  docs/reports/latest.json    机读结论（给 CI / 徽章用）
  docs/reports/media/**       拷贝过来的截图 / 视频 / 血缘树
  docs/showcase.md            展示页：截图 / 录屏 / 血缘树样例

只依赖标准库；ffmpeg 可选（存在时把帧序列合成为 mp4）。
"""

from __future__ import annotations

import argparse
import html
import json
import os
import re
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path

# ── UE 自动化状态 → 展示用文案 ────────────────────────────────────────────
STATE_LABELS = {
    "Success": "通过",
    "Fail": "失败",
    "Failed": "失败",
    "NotRun": "未执行",
    "InProcess": "执行中",
    "Skipped": "跳过",
}

BEGIN_RE = re.compile(r"Begin Test:\s*(\S+)")
RESULT_RE = re.compile(r"Test Completed\.\s*Result=\s*\{?(\w+)\}?")
ERROR_RE = re.compile(r"LogAutomationController:\s*(Error:.*)")


def parse_json_reports(report_dir: Path) -> list[dict]:
    """解析 UE 的 -ReportExportPath 产物。"""
    results: list[dict] = []

    def collect(node: dict, prefix: str = "") -> None:
        if not isinstance(node, dict):
            return

        name = node.get("testDisplayName") or node.get("displayName") or node.get("fullTestPath")
        state = node.get("state") or node.get("testStatus")
        if name and state:
            errors = []
            for entry in node.get("entries", []) or []:
                event = entry.get("event", {}) if isinstance(entry, dict) else {}
                if isinstance(event, dict) and event.get("type") in ("Error", "Warning"):
                    msg = event.get("message") or ""
                    if msg:
                        errors.append(msg)
            results.append(
                {
                    "name": str(name),
                    "state": str(state),
                    "duration": node.get("duration", 0.0),
                    "errors": errors,
                }
            )

        for child in node.get("children", []) or node.get("tests", []) or []:
            collect(child, prefix)

    candidates = []
    index = report_dir / "index.json"
    if index.is_file():
        candidates.append(index)
    candidates.extend(sorted(report_dir.glob("*.json")))

    seen = set()
    for path in candidates:
        try:
            # 注意 utf-8-sig：UE 导出的 index.json 带 BOM，用 utf-8 读会直接抛异常，
            # 被下面的 except 静默吞掉，表现为"报告里 0 个用例"。
            data = json.loads(path.read_text(encoding="utf-8-sig", errors="ignore"))
        except Exception:
            continue

        if isinstance(data, dict):
            for child in data.get("tests", []) or []:
                collect(child)
        elif isinstance(data, list):
            for child in data:
                collect(child)

    # 去重（index.json 与分片报告可能重复）
    unique = []
    for item in results:
        if item["name"] not in seen:
            seen.add(item["name"])
            unique.append(item)

    return unique


def parse_log(log_path: Path) -> list[dict]:
    """兜底：从 Automation.log 里抓结论。"""
    results: list[dict] = []
    if not log_path.is_file():
        return results

    pending: str | None = None
    for line in log_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        begin = BEGIN_RE.search(line)
        if begin:
            pending = begin.group(1)
            continue

        result = RESULT_RE.search(line)
        if result and pending:
            results.append(
                {
                    "name": pending,
                    "state": result.group(1),
                    "duration": 0.0,
                    "errors": [],
                }
            )
            pending = None
    return results


def collect_media(media_root: Path, out_dir: Path) -> dict:
    """把截图 / 帧序列 / 血缘树拷进 docs，并在有 ffmpeg 时合成视频。"""
    media = {"screenshots": [], "videos": [], "traces": []}
    if not media_root.is_dir():
        return media

    out_dir.mkdir(parents=True, exist_ok=True)

    # 截图
    for path in sorted(media_root.rglob("*.png")):
        rel = path.relative_to(media_root).as_posix()
        if "/frames/" in rel:
            continue  # 帧序列单独处理
        target = out_dir / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        media["screenshots"].append(rel)

    # 血缘树文本（headless 也一定有，是 showcase 的主力素材）
    for path in sorted(media_root.rglob("*.txt")):
        rel = path.relative_to(media_root).as_posix()
        target = out_dir / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        media["traces"].append(rel)

    # 帧序列 → mp4
    if shutil.which("ffmpeg"):
        for frames_dir in sorted(media_root.rglob("frames")):
            if not frames_dir.is_dir():
                continue
            test_name = frames_dir.parent.name
            out_file = out_dir / test_name / "recording.mp4"
            out_file.parent.mkdir(parents=True, exist_ok=True)
            cmd = [
                "ffmpeg", "-y", "-framerate", "30",
                "-pattern_type", "glob", "-i", str(frames_dir / "*.png"),
                "-c:v", "libx264", "-pix_fmt", "yuv420p",
                str(out_file),
            ]
            try:
                subprocess.run(cmd, check=True, capture_output=True)
                media["videos"].append(f"{test_name}/recording.mp4")
            except Exception:
                pass

    return media


def build_markdown(results: list[dict], media: dict, stamp: str) -> str:
    total = len(results)
    passed = sum(1 for r in results if r["state"] in ("Success",))
    failed = total - passed

    lines = [
        "# UAK 自动化报告",
        "",
        f"- 运行时间：`{stamp}`",
        f"- 用例总数：**{total}**　通过：**{passed}**　失败：**{failed}**",
        "",
        "> 本文件由 `Scripts/report/generate_report.py` 自动生成，请勿手改。",
        "",
        "## 用例明细",
        "",
        "| 结果 | 用例 | 耗时(s) |",
        "| --- | --- | --- |",
    ]

    for item in sorted(results, key=lambda r: r["name"]):
        mark = "✅" if item["state"] == "Success" else "❌"
        lines.append(
            f"| {mark} | `{item['name']}` | {item['duration']:.3f} |"
        )

    if failed:
        lines += ["", "## 失败详情", ""]
        for item in results:
            if item["state"] == "Success":
                continue
            lines.append(f"### `{item['name']}`")
            if item["errors"]:
                for err in item["errors"][:10]:
                    lines.append(f"- {err}")
            else:
                lines.append("- （无附加错误信息，请查看 Automation.log）")
            lines.append("")

    lines += [
        "",
        "## 产物",
        "",
    ]
    if media["screenshots"]:
        lines.append("### 截图")
        for rel in media["screenshots"]:
            lines.append(f"- `media/{rel}`")
        lines.append("")
    if media["videos"]:
        lines.append("### 录屏")
        for rel in media["videos"]:
            lines.append(f"- `media/{rel}`")
        lines.append("")
    if media["traces"]:
        lines.append("### 血缘树样例（headless 也能产出）")
        for rel in media["traces"]:
            lines.append(f"- `media/{rel}`")
        lines.append("")

    return "\n".join(lines) + "\n"


def build_html(results: list[dict], media: dict, stamp: str) -> str:
    total = len(results)
    passed = sum(1 for r in results if r["state"] == "Success")
    failed = total - passed

    rows = []
    for item in sorted(results, key=lambda r: r["name"]):
        ok = item["state"] == "Success"
        rows.append(
            "<tr class='{}'><td>{}</td><td><code>{}</code></td><td>{:.3f}</td></tr>".format(
                "ok" if ok else "bad",
                "通过" if ok else "失败",
                html.escape(item["name"]),
                item["duration"] or 0.0,
            )
        )

    gallery = []
    for rel in media["screenshots"]:
        gallery.append(
            f"<figure><img src='media/{html.escape(rel)}' alt='{html.escape(rel)}'/>"
            f"<figcaption>{html.escape(rel)}</figcaption></figure>"
        )
    for rel in media["videos"]:
        gallery.append(
            f"<figure><video src='media/{html.escape(rel)}' controls></video>"
            f"<figcaption>{html.escape(rel)}</figcaption></figure>"
        )

    return f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8"/>
<title>UAK 自动化报告 {html.escape(stamp)}</title>
<style>
body {{ font-family: -apple-system, "Segoe UI", "Microsoft YaHei", sans-serif; margin: 2rem; color: #222; }}
h1 {{ border-bottom: 3px solid #4f8ff7; padding-bottom: .4rem; }}
.summary {{ display: flex; gap: 1.5rem; margin: 1rem 0 2rem; }}
.card {{ padding: 1rem 1.5rem; border-radius: 10px; background: #f4f6fb; min-width: 120px; }}
.card b {{ display: block; font-size: 1.8rem; }}
table {{ border-collapse: collapse; width: 100%; }}
td, th {{ border: 1px solid #dfe3ec; padding: .45rem .7rem; text-align: left; }}
tr.ok td:first-child {{ color: #1a7f37; }}
tr.bad td:first-child {{ color: #b42318; }}
figure {{ display: inline-block; margin: .5rem; max-width: 480px; }}
figure img, figure video {{ width: 100%; border-radius: 8px; border: 1px solid #dfe3ec; }}
figcaption {{ font-size: .85rem; color: #666; margin-top: .3rem; }}
</style>
</head>
<body>
<h1>UAK 自动化报告</h1>
<p>运行时间：{html.escape(stamp)}</p>
<div class="summary">
  <div class="card"><b>{total}</b>用例总数</div>
  <div class="card"><b style="color:#1a7f37">{passed}</b>通过</div>
  <div class="card"><b style="color:#b42318">{failed}</b>失败</div>
</div>
<table>
<tr><th>结果</th><th>用例</th><th>耗时(s)</th></tr>
{''.join(rows)}
</table>
<h2>产物</h2>
{''.join(gallery) or '<p>（无截图 / 录屏；headless 运行只会产出血缘树文本）</p>'}
</body>
</html>
"""


def build_showcase(results: list[dict], media: dict, stamp: str) -> str:
    lines = [
        "# Showcase",
        "",
        "这一页由 `Scripts/report/generate_report.py` 自动生成——素材全部来自自动化测试，",
        "**不是手写的宣传页**：测试跑出来的截图、录屏与血缘树，直接就是展示内容。",
        "",
        f"最近一次运行：`{stamp}`　（用例 {len(results)} 个）",
        "",
    ]

    if media["screenshots"] or media["videos"]:
        lines += ["## 截图与录屏", ""]
        for rel in media["screenshots"]:
            lines += [f"![{rel}](reports/media/{rel})", ""]
        for rel in media["videos"]:
            lines += [f"🎬 [{rel}](reports/media/{rel})", ""]
    else:
        lines += [
            "## 截图与录屏",
            "",
            "本次运行没有开启媒体采集（headless 默认关闭）。",
            "在本机执行 `Scripts/run_tests.ps1 -CaptureMedia` 即可产出截图与录屏。",
            "",
        ]

    if media["traces"]:
        lines += ["## 血缘树（Explain）", "", "UAK 独有的能力：一次施放的完整因果链，可以直接下钻。", ""]
        for rel in media["traces"]:
            lines += [f"### `{rel}`", "", "```text"]
            trace_file = Path("docs/reports/media") / rel
            if trace_file.is_file():
                lines.append(trace_file.read_text(encoding="utf-8", errors="ignore").rstrip())
            lines += ["```", ""]

    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="生成 UAK 自动化测试报告")
    parser.add_argument("--project", required=True, help="工程根目录")
    parser.add_argument("--report-dir", default="", help="UE 自动化报告目录（-ReportExportPath）")
    parser.add_argument("--log-file", default="", help="Automation.log 路径（兜底）")
    parser.add_argument("--media-root", default="", help="测试产物目录（截图 / 帧 / 血缘树）")
    parser.add_argument("--out", default="docs/reports", help="报告输出目录（相对工程根）")
    parser.add_argument("--stamp", default="", help="运行时间戳")
    args = parser.parse_args()

    root = Path(args.project).resolve()
    stamp = args.stamp or datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    report_dir = Path(args.report_dir) if args.report_dir else root / "Saved" / "Automation" / "Reports"
    log_file = Path(args.log_file) if args.log_file else root / "Saved" / "Automation" / "Logs" / "Automation.log"
    media_root = Path(args.media_root) if args.media_root else root / "Saved" / "UAKAutomation"

    results = parse_json_reports(report_dir)
    if not results:
        results = parse_log(log_file)

    out_dir = root / args.out
    out_dir.mkdir(parents=True, exist_ok=True)
    media = collect_media(media_root, out_dir / "media")

    (out_dir / "latest.md").write_text(build_markdown(results, media, stamp), encoding="utf-8")
    (out_dir / "latest.html").write_text(build_html(results, media, stamp), encoding="utf-8")
    (out_dir / "latest.json").write_text(
        json.dumps(
            {
                "stamp": stamp,
                "total": len(results),
                "passed": sum(1 for r in results if r["state"] == "Success"),
                "failed": sum(1 for r in results if r["state"] != "Success"),
                "tests": results,
                "media": media,
            },
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )

    docs_dir = root / "docs"
    docs_dir.mkdir(parents=True, exist_ok=True)
    (docs_dir / "showcase.md").write_text(build_showcase(results, media, stamp), encoding="utf-8")

    passed = sum(1 for r in results if r["state"] == "Success")
    print(f"[UAK] 报告已生成：{out_dir}")
    print(f"[UAK] 用例 {len(results)} 个，通过 {passed}，失败 {len(results) - passed}")

    # 没有用例时视为异常（通常是没跑到自动化，而不是"全过了"）
    if len(results) == 0:
        print("[UAK] 警告：没有解析到任何用例结论，请检查 ReportExportPath / Automation.log", file=sys.stderr)
        return 2

    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
