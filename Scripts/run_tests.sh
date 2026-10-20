#!/usr/bin/env bash
#
# UAK 测试启动脚本（Linux / macOS）
# 用法：
#   ./Scripts/run_tests.sh                 # 全量跑
#   ./Scripts/run_tests.sh --filter UAK.Unit
#   ./Scripts/run_tests.sh --skip-build
#   ./Scripts/run_tests.sh --capture-media # 需要真实 RHI
#
# 与 run_tests.ps1 等价：编译 → 跑 Automation → 生成报告 / 素材 / 文档。
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

ENGINE_ROOT="${UE_ENGINE_ROOT:-${1:-}}"
FILTER="${FILTER:-UAK}"
SKIP_BUILD=0
CAPTURE_MEDIA=0
PYTHON_BIN="${PYTHON:-python3}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --engine-root)   ENGINE_ROOT="$2"; shift 2 ;;
    --filter)        FILTER="$2"; shift 2 ;;
    --project)       PROJECT_ROOT="$2"; shift 2 ;;
    --skip-build)    SKIP_BUILD=1; shift ;;
    --capture-media) CAPTURE_MEDIA=1; shift ;;
    --python)        PYTHON_BIN="$2"; shift 2 ;;
    -h|--help)       sed -n '2,14p' "$0"; exit 0 ;;
    *) echo "未知参数：$1" >&2; exit 2 ;;
  esac
done

UPROJECT="$PROJECT_ROOT/UAKDemo.uproject"
[[ -f "$UPROJECT" ]] || { echo "找不到工程文件：$UPROJECT" >&2; exit 1; }
[[ -n "$ENGINE_ROOT" ]] || { echo "请用 --engine-root 指定引擎目录，或设置 UE_ENGINE_ROOT" >&2; exit 1; }

UBT="$ENGINE_ROOT/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll"
EDITOR="$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd"
[[ -f "$UBT" ]]    || { echo "缺少 UBT：$UBT" >&2; exit 1; }
[[ -f "$EDITOR" ]] || { echo "缺少编辑器：$EDITOR" >&2; exit 1; }

STAMP="$(date +%Y%m%d-%H%M%S)"
REPORT_DIR="$PROJECT_ROOT/Saved/Automation/Reports/$STAMP"
LOG_DIR="$PROJECT_ROOT/Saved/Automation/Logs"
LOG_FILE="$LOG_DIR/Automation-$STAMP.log"
mkdir -p "$REPORT_DIR" "$LOG_DIR"

echo "[UAK] 工程：$PROJECT_ROOT"
echo "[UAK] 引擎：$ENGINE_ROOT"
echo "[UAK] 过滤：$FILTER"

if [[ "$SKIP_BUILD" -eq 0 ]]; then
  echo "[UAK] 编译 UAKDemoEditor (Linux/Development)…"
  (cd "$ENGINE_ROOT/Engine/Source" && dotnet "$UBT" UAKDemoEditor Linux Development "-Project=$UPROJECT" -WaitMutex)
fi

EDITOR_ARGS=(
  "$UPROJECT"
  "-ExecCmds=Automation RunTests $FILTER; Quit"
  "-TestExit=Automation Test Queue Empty"
  "-ReportExportPath=$REPORT_DIR"
  -unattended -nopause -nosplash
  "-abslog=$LOG_FILE"
)
if [[ "$CAPTURE_MEDIA" -eq 1 ]]; then
  EDITOR_ARGS+=(-Windowed -ResX=1280 -ResY=720 -UAKCaptureMedia=1)
else
  EDITOR_ARGS+=(-NullRHI)
fi

echo "[UAK] 运行自动化：$FILTER"
set +e
"$EDITOR" "${EDITOR_ARGS[@]}"
EDITOR_EXIT=$?
set -e
echo "[UAK] 编辑器退出码：$EDITOR_EXIT"

if command -v "$PYTHON_BIN" >/dev/null 2>&1; then
  "$PYTHON_BIN" "$SCRIPT_DIR/report/generate_report.py" \
    --project "$PROJECT_ROOT" \
    --report-dir "$REPORT_DIR" \
    --log-file "$LOG_FILE" \
    --out docs/reports \
    --stamp "$(date '+%Y-%m-%d %H:%M:%S')"
  REPORT_EXIT=$?
else
  echo "[UAK] 未找到 python3，跳过报告生成（原始数据仍在 $REPORT_DIR）" >&2
  REPORT_EXIT=0
fi

echo "[UAK] 报告输出：$PROJECT_ROOT/docs/reports"
exit "$REPORT_EXIT"
