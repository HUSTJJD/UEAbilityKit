#!/usr/bin/env bash
# UAK 一键教程流水线：录制编辑器操作过程 → 产出截图 / 录屏 → 生成教程文档。
#
# 与 run_tests.sh 一样，只在本地跑（要有 UE + 真实 RHI 才有像素）；
# CI 只负责把生成的 docs/tutorials 渲染到 GitHub Pages。
#
# 用法：
#   ./Scripts/generate_tutorials.sh                  # 录制全部教程并生成文档
#   ./Scripts/generate_tutorials.sh --docs-only      # 只用已有产物重新生成文档
#   ./Scripts/generate_tutorials.sh --no-capture     # 只验证步骤，不采素材
#   ./Scripts/generate_tutorials.sh --tutorial 01-first-ability
set -euo pipefail

PROJECT=""
ENGINE_ROOT="${UE_ENGINE_ROOT:-}"
PYTHON="${PYTHON:-python3}"
DOCS_ONLY=0
NO_CAPTURE=0
TUTORIAL=""
TIMEOUT_SECONDS=1200
COMMIT=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --project)        PROJECT="$2"; shift 2 ;;
    --engine-root)    ENGINE_ROOT="$2"; shift 2 ;;
    --python)         PYTHON="$2"; shift 2 ;;
    --docs-only)      DOCS_ONLY=1; shift ;;
    --no-capture)     NO_CAPTURE=1; shift ;;
    --tutorial)       TUTORIAL="$2"; shift 2 ;;
    --timeout)        TIMEOUT_SECONDS="$2"; shift 2 ;;
    --commit)         COMMIT=1; shift ;;
    -h|--help)        sed -n '2,14p' "$0"; exit 0 ;;
    *) echo "未知参数：$1" >&2; exit 2 ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT="${PROJECT:-$(cd "$SCRIPT_DIR/.." && pwd)}"
PROJECT="$(cd "$PROJECT" && pwd)"
GENERATOR="$SCRIPT_DIR/tutorial/generate_tutorials.py"

run_generator() {
  echo "[UAK] 生成教程文档…"
  "$PYTHON" "$GENERATOR" --project "$PROJECT"
}

if [[ ! -d "$PROJECT/Tutorials" ]]; then
  echo "[UAK] 找不到教程脚本目录：$PROJECT/Tutorials" >&2
  exit 2
fi

if [[ "$DOCS_ONLY" -eq 1 ]]; then
  run_generator
  exit 0
fi

UPROJECT="$PROJECT/LyraStarterGame.uproject"
if [[ ! -f "$UPROJECT" ]]; then
  UPROJECT="$(find "$PROJECT" -maxdepth 1 -name '*.uproject' | head -n 1 || true)"
fi
if [[ -z "$UPROJECT" || ! -f "$UPROJECT" ]]; then
  echo "[UAK] 找不到 .uproject，请先跑 Tools/import_lyra.sh 导入 Lyra" >&2
  exit 2
fi

if [[ -z "$ENGINE_ROOT" || ! -d "$ENGINE_ROOT" ]]; then
  echo "[UAK] 找不到 UE 引擎：请用 --engine-root 指定，或设置 UE_ENGINE_ROOT" >&2
  exit 2
fi

# 教程要截资产编辑器面板，所以需要完整编辑器（不是 -Cmd）
case "$(uname -s)" in
  Darwin) EDITOR="$ENGINE_ROOT/Engine/Binaries/Mac/UnrealEditor" ;;
  *)      EDITOR="$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor" ;;
esac
if [[ ! -x "$EDITOR" ]]; then
  echo "[UAK] 找不到编辑器：$EDITOR" >&2
  exit 2
fi

CMDS=()
[[ "$NO_CAPTURE" -eq 1 ]] && CMDS+=("UAK.Tutorial.Capture 0")
if [[ -n "$TUTORIAL" ]]; then
  CMDS+=("UAK.Tutorial.Run $TUTORIAL")
else
  CMDS+=("UAK.Tutorial.RunAll -Quit")
fi
EXEC_CMDS="$(IFS='; '; echo "${CMDS[*]}")"

echo "[UAK] 启动编辑器录制教程：$EXEC_CMDS"
timeout "$TIMEOUT_SECONDS" "$EDITOR" "$UPROJECT" \
  -ExecCmds="$EXEC_CMDS" -unattended -nosplash -stdout -FullStdOutLogOutput || {
    echo "[UAK] 编辑器退出码非 0 或超时；已产出的步骤仍会写进 run.json" >&2
  }

run_generator

if [[ "$COMMIT" -eq 1 ]]; then
  (cd "$PROJECT" && git add docs/tutorials Tutorials mkdocs.yml \
    && git commit -m "docs(tutorials): 重新录制并生成编辑器教程")
  echo "[UAK] 已提交；CI 会把它渲染到 Pages"
fi
