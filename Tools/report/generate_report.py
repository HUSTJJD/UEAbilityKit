#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
UAK 自动化报告生成器。

输入（全部由 Tools/run_tests.ps1 / .sh 在本地产生）：
  1) UE 的自动化报告：<ReportDir>/index.json（或若干 Report-*.json），可给多个目录
     兜底：解析 Saved/Automation/Logs/Automation*.log
  2) 进程外 e2e 的标记行：日志里的 "UAK_E2E <Action> <Name> <OK|FAIL|SKIP> [k=v ...]"
     （由 Runtime 模块的 UAK.Auto.* 命令打出）
  3) 多端快照：Saved/UAKAutomation/Net/<Role>.json（DS / 客户端各自的 GAS 状态）
  4) 测试产物：Saved/UAKAutomation/ 下的截图、帧序列、血缘树文本

输出（提交回仓库，CI 只负责渲染成 Pages）：
  docs/reports/latest.md      结论（Markdown，给 GitHub 看）
  docs/reports/latest.html    结论（自包含 HTML，给 GitHub Pages 看）
  docs/reports/latest.json    机读结论（给徽章 / 后续流水线用）
  docs/reports/media/**       拷贝过来的截图 / 视频
  docs/showcase.md            展示页：截图 / 录屏 / 血缘树 / 多端一致性

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

# Runtime 模块 UAK.Auto.* 打出的机器可读标记
MARKER_RE = re.compile(r"UAK_E2E\s+(\S+)\s+(\S+)\s+(OK|FAIL|SKIP)(.*)")
_KV_RE = re.compile(r"(\w+)=(\S+)")

# 多端一致性比对容差（属性是浮点，允许极小误差）
NET_EPSILON = 0.01


def parse_json_reports(report_dirs: list[Path]) -> list[dict]:
    """解析 UE 的 -ReportExportPath 产物（可来自多个 stage 的多个目录）。"""
    results: list[dict] = []

    def collect(node: dict) -> None:
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
                    "duration": node.get("duration", 0.0) or 0.0,
                    "errors": errors,
                }
            )

        for child in node.get("children", []) or node.get("tests", []) or []:
            collect(child)

    for report_dir in report_dirs:
        if not report_dir or not report_dir.is_dir():
            continue

        candidates = []
        index = report_dir / "index.json"
        if index.is_file():
            candidates.append(index)
        candidates.extend(sorted(report_dir.glob("*.json")))

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

    # 去重（index.json 与分片报告可能重复）；同名时保留"更差"的结果
    merged: dict[str, dict] = {}
    for item in results:
        old = merged.get(item["name"])
        if old is None or (old["state"] == "Success" and item["state"] != "Success"):
            merged[item["name"]] = item

    return list(merged.values())


def parse_log(log_paths: list[Path]) -> list[dict]:
    """兜底：从 Automation.log 里抓结论（没拿到 JSON 报告时）。"""
    results: list[dict] = []
    for log_path in log_paths:
        if not log_path or not log_path.is_file():
            continue

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


def parse_e2e_markers(log_paths: list[Path]) -> list[dict]:
    """
    解析进程外 e2e 的标记行。

    这些测试跑在 -game / -server 进程里（Editor 类型的测试模块不一定加载），
    因此没有 UE 的 JSON 报告，只能靠日志里的 UAK_E2E 行还原出"用例"。
    """
    rows: dict[str, dict] = {}

    for log_path in log_paths:
        if not log_path or not log_path.is_file():
            continue

        for line in log_path.read_text(encoding="utf-8", errors="ignore").splitlines():
            match = MARKER_RE.search(line)
            if not match:
                continue

            action, name, status, extra = match.groups()
            state = {"OK": "Success", "FAIL": "Fail", "SKIP": "Skipped"}[status]
            test_name = f"UAK.E2E.{action}.{name}"
            detail = dict((k, v) for k, v in _KV_RE.findall(extra or ""))

            old = rows.get(test_name)
            if old is None:
                rows[test_name] = {
                    "name": test_name,
                    "state": state,
                    "duration": 0.0,
                    "errors": [] if state == "Success" else [json.dumps(detail, ensure_ascii=False)],
                    "detail": detail,
                }
            elif old["state"] == "Success" and state != "Success":
                # 同名标记取最差的结论
                rows[test_name] = {
                    "name": test_name,
                    "state": state,
                    "duration": 0.0,
                    "errors": [json.dumps(detail, ensure_ascii=False)],
                    "detail": detail,
                }

    return list(rows.values())


def _actor_values(snapshot: dict) -> dict[str, list[float]]:
    """角色 → {属性名: [各 Actor 上的值（排序后）]}。

    不同进程里同一个 Actor 的名字不一样（BP_DemoCharacter_C_0 / _1），
    所以用"值的多重集合"来比对，而不是按 Actor 名字配对。
    """
    out: dict[str, list[float]] = {}
    for actor in snapshot.get("actors", []) or []:
        for attr, value in (actor.get("attributes") or {}).items():
            if not isinstance(value, (int, float)):
                continue
            out.setdefault(attr, []).append(round(float(value), 4))
    for values in out.values():
        values.sort()
    return out


def compare_net(net_dir: Path) -> dict:
    """
    多端一致性：把 DS 与各客户端的 GAS 快照对齐，属性必须收敛到同一个值。

    这是 DS / 多客户端 e2e 的主断言——UAK 不写网络代码（docs/design/13-network.md），
    正确性体现在"所有端看到的属性都一样"，而不是"录到了什么画面"。
    """
    report = {"roles": [], "comparisons": [], "rows": [], "snapshots": {}}
    if not net_dir.is_dir():
        return report

    snapshots: dict[str, dict] = {}
    for path in sorted(net_dir.glob("*.json")):
        try:
            data = json.loads(path.read_text(encoding="utf-8-sig", errors="ignore"))
        except Exception:
            continue
        role = data.get("role") or path.stem
        snapshots[role] = data

    report["roles"] = sorted(snapshots.keys())
    report["snapshots"] = {
        role: {
            "map": data.get("map"),
            "time": data.get("time"),
            "netMode": data.get("netMode"),
            "actors": len(data.get("actors") or []),
        }
        for role, data in snapshots.items()
    }

    if len(snapshots) < 2:
        report["rows"].append(
            {
                "name": "UAK.Net.Converge.Snapshots",
                "state": "Fail",
                "duration": 0.0,
                "errors": [f"只拿到 {len(snapshots)} 份快照，至少需要 2 份（DS + 客户端）"],
            }
        )
        return report

    # 以"权威端"（名字里带 Server / 或 netMode == 1）为基准，其余端逐个比对
    def is_authority(data: dict) -> bool:
        return (data.get("netMode") == 1) or ("server" in str(data.get("role", "")).lower())

    base_role = next((r for r, d in snapshots.items() if is_authority(d)), sorted(snapshots.keys())[0])
    base = _actor_values(snapshots[base_role])

    for role, data in sorted(snapshots.items()):
        if role == base_role:
            continue

        other = _actor_values(data)
        mismatches = []
        for attr, values in sorted(base.items()):
            if attr not in other:
                mismatches.append(f"{attr}: 客户端缺失")
                continue
            if len(values) != len(other[attr]):
                mismatches.append(f"{attr}: 数量不一致（{len(values)} vs {len(other[attr])}）")
                continue
            for a, b in zip(values, other[attr]):
                if abs(a - b) > NET_EPSILON:
                    mismatches.append(f"{attr}: {base_role}={a} {role}={b}")

        report["comparisons"].append(
            {
                "base": base_role,
                "role": role,
                "attributes": sorted(base.keys()),
                "mismatches": mismatches,
            }
        )
        report["rows"].append(
            {
                "name": f"UAK.Net.Converge.{role}",
                "state": "Success" if not mismatches else "Fail",
                "duration": 0.0,
                "errors": mismatches[:10],
            }
        )

    return report


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


def _mark(item: dict) -> str:
    if item["state"] == "Success":
        return "✅"
    if item["state"] == "Skipped":
        return "⏭️"
    return "❌"


def build_markdown(results: list[dict], media: dict, net: dict, stamp: str) -> str:
    total = len(results)
    passed = sum(1 for r in results if r["state"] == "Success")
    failed = sum(1 for r in results if r["state"] != "Success")

    lines = [
        "# UAK 自动化报告",
        "",
        f"- 运行时间：`{stamp}`",
        f"- 用例总数：**{total}**　通过：**{passed}**　失败：**{failed}**",
        "",
        "> 本文件由 `Tools/report/generate_report.py` 自动生成，请勿手改。",
        "> 测试在本地一键运行（`Tools/run_tests.ps1`），CI 只负责把结论渲染到 Pages。",
        "",
        "## 用例明细",
        "",
        "| 结果 | 用例 | 耗时(s) |",
        "| --- | --- | --- |",
    ]

    for item in sorted(results, key=lambda r: r["name"]):
        lines.append(f"| {_mark(item)} | `{item['name']}` | {item['duration']:.3f} |")

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

    # 多端一致性
    if net.get("roles"):
        lines += ["", "## 多端一致性（DS / 多客户端）", ""]
        lines.append(f"- 参与的端：{', '.join(f'`{r}`' for r in net['roles'])}")
        for item in net.get("comparisons", []):
            state = "一致" if not item["mismatches"] else "不一致"
            lines.append(f"- `{item['base']}` ↔ `{item['role']}`：**{state}**（属性 {len(item['attributes'])} 项）")
            for miss in item["mismatches"][:10]:
                lines.append(f"  - ❌ {miss}")
        lines.append("")

    lines += ["", "## 产物", ""]
    if media["videos"]:
        lines.append("### 录屏")
        for rel in media["videos"]:
            lines.append(f"- `media/{rel}`")
        lines.append("")
    if media["screenshots"]:
        lines.append("### 截图")
        for rel in media["screenshots"]:
            lines.append(f"- `media/{rel}`")
        lines.append("")
    if media["traces"]:
        lines.append("### 血缘树样例（headless 也能产出）")
        for rel in media["traces"]:
            lines.append(f"- `media/{rel}`")
        lines.append("")

    return "\n".join(lines) + "\n"


def build_html(results: list[dict], media: dict, net: dict, stamp: str) -> str:
    total = len(results)
    passed = sum(1 for r in results if r["state"] == "Success")
    failed = total - passed

    rows = []
    for item in sorted(results, key=lambda r: r["name"]):
        ok = item["state"] == "Success"
        rows.append(
            "<tr class='{}'><td>{}</td><td><code>{}</code></td><td>{:.3f}</td></tr>".format(
                "ok" if ok else "bad",
                STATE_LABELS.get(item["state"], item["state"]),
                html.escape(item["name"]),
                item["duration"] or 0.0,
            )
        )

    net_rows = []
    for item in net.get("comparisons", []):
        ok = not item["mismatches"]
        net_rows.append(
            "<tr class='{}'><td>{}</td><td><code>{}</code> ↔ <code>{}</code></td><td>{}</td></tr>".format(
                "ok" if ok else "bad",
                "一致" if ok else "不一致",
                html.escape(item["base"]),
                html.escape(item["role"]),
                "" if ok else html.escape("；".join(item["mismatches"][:3])),
            )
        )

    gallery = []
    for rel in media["videos"]:
        gallery.append(
            f"<figure><video src='media/{html.escape(rel)}' controls></video>"
            f"<figcaption>{html.escape(rel)}</figcaption></figure>"
        )
    for rel in media["screenshots"]:
        gallery.append(
            f"<figure><img src='media/{html.escape(rel)}' alt='{html.escape(rel)}'/>"
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
<h2>多端一致性</h2>
{('<table><tr><th>结论</th><th>对比</th><th>差异</th></tr>' + ''.join(net_rows) + '</table>')
    if net_rows else '<p>（本次没有跑 DS / 多客户端 stage）</p>'}
<h2>产物</h2>
{''.join(gallery) or '<p>（无截图 / 录屏；headless 运行只会产出血缘树文本）</p>'}
</body>
</html>
"""


def build_showcase(results: list[dict], media: dict, net: dict, stamp: str) -> str:
    lines = [
        "# Showcase",
        "",
        "这一页由 `Tools/report/generate_report.py` 自动生成——素材全部来自本地一键跑出来的测试，",
        "**不是手写的宣传页**：截图、录屏、血缘树、多端一致性，都是测试的直接产物。",
        "",
        f"最近一次运行：`{stamp}`　（用例 {len(results)} 个）",
        "",
    ]

    if media["videos"] or media["screenshots"]:
        lines += ["## 录屏与截图", ""]
        for rel in media["videos"]:
            lines += [f"🎬 [{rel}](reports/media/{rel})", ""]
        for rel in media["screenshots"]:
            lines += [f"![{rel}](reports/media/{rel})", ""]
    else:
        lines += [
            "## 录屏与截图",
            "",
            "本次运行没有产出媒体（headless / `-Headless` 运行没有像素）。",
            "在本机执行 `Tools/run_tests.ps1` 即可产出截图与录屏。",
            "",
        ]

    if net.get("roles"):
        lines += ["## 多端一致性（DS + 多客户端）", ""]
        lines.append("| 基准端 | 对比端 | 结论 |")
        lines.append("| --- | --- | --- |")
        for item in net.get("comparisons", []):
            ok = not item["mismatches"]
            lines.append(f"| `{item['base']}` | `{item['role']}` | {'✅ 属性一致' if ok else '❌ 属性不一致'} |")
        lines.append("")

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
    parser.add_argument("--report-dir", action="append", default=[], help="UE 自动化报告目录（可重复，-ReportExportPath）")
    parser.add_argument("--log-file", action="append", default=[], help="日志文件（可重复；解析 UAK_E2E 标记与兜底结论）")
    parser.add_argument("--media-root", default="", help="测试产物目录（截图 / 帧 / 血缘树）")
    parser.add_argument("--net-dir", default="", help="多端快照目录（默认 <media-root>/Net）")
    parser.add_argument("--out", default="docs/reports", help="报告输出目录（相对工程根）")
    parser.add_argument("--stamp", default="", help="运行时间戳")
    args = parser.parse_args()

    root = Path(args.project).resolve()
    stamp = args.stamp or datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    report_dirs = [Path(p) for p in args.report_dir if p]
    log_files = [Path(p) for p in args.log_file if p]
    if not report_dirs:
        report_dirs = [root / "Saved" / "Automation" / "Reports"]
    if not log_files:
        log_files = [root / "Saved" / "Automation" / "Logs" / "Automation.log"]

    media_root = Path(args.media_root) if args.media_root else root / "Saved" / "UAKAutomation"
    net_dir = Path(args.net_dir) if args.net_dir else media_root / "Net"

    results = parse_json_reports(report_dirs)
    if not results:
        results = parse_log(log_files)

    # 进程外 e2e（UAK.Auto.*）没有 JSON 报告，从日志标记还原
    results += parse_e2e_markers(log_files)

    net = compare_net(net_dir)
    # 多端一致性也是结论的一部分：失败要能体现在总数与退出码里
    results += net["rows"]

    out_dir = root / args.out
    out_dir.mkdir(parents=True, exist_ok=True)
    media = collect_media(media_root, out_dir / "media")

    passed = sum(1 for r in results if r["state"] == "Success")

    (out_dir / "latest.md").write_text(build_markdown(results, media, net, stamp), encoding="utf-8")
    (out_dir / "latest.html").write_text(build_html(results, media, net, stamp), encoding="utf-8")
    (out_dir / "latest.json").write_text(
        json.dumps(
            {
                "stamp": stamp,
                "total": len(results),
                "passed": passed,
                "failed": len(results) - passed,
                "tests": results,
                "media": media,
                "net": net,
            },
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )

    docs_dir = root / "docs"
    docs_dir.mkdir(parents=True, exist_ok=True)
    (docs_dir / "showcase.md").write_text(build_showcase(results, media, net, stamp), encoding="utf-8")

    print(f"[UAK] 报告已生成：{out_dir}")
    print(f"[UAK] 用例 {len(results)} 个，通过 {passed}，失败 {len(results) - passed}")
    if net.get("roles"):
        print(f"[UAK] 多端一致性：{', '.join(net['roles'])}")

    # 没有用例时视为异常（通常是没跑到自动化，而不是"全过了"）
    if len(results) == 0:
        print("[UAK] 警告：没有解析到任何用例结论，请检查 ReportExportPath / 日志", file=sys.stderr)
        return 2

    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
