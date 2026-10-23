#!/usr/bin/env bash
#
# UAK 一键测试（Linux / macOS），与 run_tests.ps1 等价。
#
# 测试只在本地跑；CI 不跑 UE，只把产出的 docs/reports 与 docs/showcase.md 渲染到 Pages。
#
# 用法：
#   ./Tools/run_tests.sh                       # 全部 stage（默认开采集）
#   ./Tools/run_tests.sh --stage editor --filter UAK.Unit
#   ./Tools/run_tests.sh --headless            # 没 GPU：只跑断言，不出素材
#   ./Tools/run_tests.sh --stage net --clients 3 --ability-class /Script/LyraAdapt.XXX
#   ./Tools/run_tests.sh --commit              # 跑完把结论提交
#
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

ENGINE_ROOT="${UE_ENGINE_ROOT:-}"
STAGE="all"
HEADLESS=0
SKIP_BUILD=0
FILTER="UAK"
CLIENTS=2
PORT=7777
MAP=""
ABILITY_CLASS=""
GAME_SECONDS=12
NET_SECONDS=25
SERVER_BOOT_WAIT=10
PYTHON_BIN="${PYTHON:-python3}"
DO_COMMIT=0
DO_PUSH=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --engine-root)    ENGINE_ROOT="$2"; shift 2 ;;
    --project)        PROJECT_ROOT="$2"; shift 2 ;;
    --stage)          STAGE="$(echo "$2" | tr '[:upper:]' '[:lower:]')"; shift 2 ;;
    --headless)       HEADLESS=1; shift ;;
    --skip-build)     SKIP_BUILD=1; shift ;;
    --filter)         FILTER="$2"; shift 2 ;;
    --clients)        CLIENTS="$2"; shift 2 ;;
    --port)           PORT="$2"; shift 2 ;;
    --map)            MAP="$2"; shift 2 ;;
    --ability-class)  ABILITY_CLASS="$2"; shift 2 ;;
    --python)         PYTHON_BIN="$2"; shift 2 ;;
    --commit)         DO_COMMIT=1; shift ;;
    --push)           DO_COMMIT=1; DO_PUSH=1; shift ;;
    -h|--help)        sed -n '2,20p' "$0"; exit 0 ;;
    *) echo "未知参数：$1" >&2; exit 2 ;;
  esac
done

PROJECT_ROOT="$(cd "$PROJECT_ROOT" && pwd)"
# 本工程 = 改造过后的 Lyra，所以就是 LyraStarterGame.uproject；
# 没跑过 import_lyra 时仓库里没有工程文件，直接提示。
UPROJECT="$PROJECT_ROOT/LyraStarterGame.uproject"
[[ -f "$UPROJECT" ]] || UPROJECT="$(find "$PROJECT_ROOT" -maxdepth 1 -name '*.uproject' | head -n 1)"
[[ -f "$UPROJECT" ]] || { echo "没有 .uproject，请先跑 Tools/import_lyra.sh 导入 Lyra" >&2; exit 1; }

EDITOR_TARGET="$(basename "$UPROJECT" .uproject)Editor"
[[ "$(basename "$UPROJECT" .uproject)" == "LyraStarterGame" ]] && EDITOR_TARGET="LyraEditor"
[[ -n "$ENGINE_ROOT" ]] || { echo "请用 --engine-root 指定引擎目录，或设置 UE_ENGINE_ROOT" >&2; exit 1; }

UBT="$ENGINE_ROOT/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll"
EDITOR="$ENGINE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd"
[[ -f "$UBT" ]]    || { echo "缺少 UBT：$UBT" >&2; exit 1; }
[[ -f "$EDITOR" ]] || { echo "缺少编辑器：$EDITOR" >&2; exit 1; }

STAMP="$(date +%Y%m%d-%H%M%S)"
BASE_DIR="$PROJECT_ROOT/Saved/Automation"
REPORT_DIR="$BASE_DIR/Reports/$STAMP"
LOG_DIR="$BASE_DIR/Logs"
mkdir -p "$REPORT_DIR" "$LOG_DIR"

REPORT_DIRS=()
LOG_FILES=()

echo "[UAK] 工程：$PROJECT_ROOT"
echo "[UAK] 引擎：$ENGINE_ROOT"
echo "[UAK] stage：$STAGE（headless=$HEADLESS）"
if [[ "$HEADLESS" -eq 0 ]] && ! command -v ffmpeg >/dev/null 2>&1; then
  echo "[UAK] 警告：没找到 ffmpeg，帧序列不会合成 mp4（截图仍会产出）" >&2
fi

# ── 编译 ────────────────────────────────────────────────────────────────
if [[ "$SKIP_BUILD" -eq 0 ]]; then
  echo "[UAK] 编译 $EDITOR_TARGET (Linux/Development)…"
  (cd "$ENGINE_ROOT/Engine/Source" && dotnet "$UBT" "$EDITOR_TARGET" Linux Development "-Project=$UPROJECT" -WaitMutex) \
    || { echo "编译失败" >&2; exit 1; }
fi

# 起一个进程，最多等 N 秒，超时就杀
run_limited() {
  local seconds="$1"; shift
  echo "[UAK] 启动：$*"
  "$@" &
  local pid=$!
  if [[ "$seconds" -gt 0 ]]; then
    ( sleep "$seconds"; kill "$pid" 2>/dev/null ) &
    local killer=$!
    wait "$pid" 2>/dev/null
    kill "$killer" 2>/dev/null
  else
    wait "$pid"
  fi
}

# ── Stage 1：编辑器内 Automation ────────────────────────────────────────
if [[ "$STAGE" == "all" || "$STAGE" == "editor" ]]; then
  STAGE_REPORT="$REPORT_DIR/Editor"
  STAGE_LOG="$LOG_DIR/Editor-$STAMP.log"
  mkdir -p "$STAGE_REPORT"

  ARGS=(
    "$UPROJECT"
    "-ExecCmds=Automation RunTests $FILTER; Quit"
    "-TestExit=Automation Test Queue Empty"
    "-ReportExportPath=$STAGE_REPORT"
    -unattended -nopause -nosplash
    "-abslog=$STAGE_LOG"
  )
  if [[ "$HEADLESS" -eq 1 ]]; then
    ARGS+=(-NullRHI)
  else
    ARGS+=(-Windowed -ResX=1280 -ResY=720 -UAKCaptureMedia=1)
  fi

  run_limited 0 "$EDITOR" "${ARGS[@]}"
  REPORT_DIRS+=("$STAGE_REPORT")
  LOG_FILES+=("$STAGE_LOG")
fi

# ── Stage 2：独立进程（-game）里的真实游戏世界 ──────────────────────────
if [[ "$STAGE" == "all" || "$STAGE" == "game" ]]; then
  STAGE_LOG="$LOG_DIR/Game-$STAMP.log"

  EXEC_CMDS=()
  [[ -n "$MAP" ]] && EXEC_CMDS+=("open $MAP")
  EXEC_CMDS+=("UAK.Auto.Snapshot Client0 2")
  EXEC_CMDS+=("UAK.Auto.Record Game.Playback 300 30")
  [[ -n "$ABILITY_CLASS" ]] && EXEC_CMDS+=("UAK.Auto.ActivateAbility $ABILITY_CLASS 3")
  EXEC="$(IFS='; '; echo "${EXEC_CMDS[*]}")"

  ARGS=(
    "$UPROJECT" -game
    "-ExecCmds=$EXEC"
    -unattended -nopause -nosplash
    "-abslog=$STAGE_LOG"
  )
  if [[ "$HEADLESS" -eq 1 ]]; then
    ARGS+=(-NullRHI)
  else
    ARGS+=(-Windowed -ResX=1280 -ResY=720)
  fi

  run_limited "$GAME_SECONDS" "$EDITOR" "${ARGS[@]}"
  LOG_FILES+=("$STAGE_LOG")
fi

# ── Stage 3：DS + N 个客户端 ────────────────────────────────────────────
if [[ "$STAGE" == "all" || "$STAGE" == "net" ]]; then
  SERVER_LOG="$LOG_DIR/Server-$STAMP.log"
  echo "[UAK] 启动：DS（端口 $PORT）"
  "$EDITOR" "$UPROJECT" -server "-Port=$PORT" \
    -ExecCmds="UAK.Auto.Snapshot Server 2" \
    -unattended -nopause -nosplash -NullRHI "-abslog=$SERVER_LOG" &
  SERVER_PID=$!
  LOG_FILES+=("$SERVER_LOG")
  sleep "$SERVER_BOOT_WAIT"

  CLIENT_PIDS=()
  for ((i=1; i<=CLIENTS; i++)); do
    CLIENT_LOG="$LOG_DIR/Client$i-$STAMP.log"
    EXEC_CMDS=(
      "open 127.0.0.1:$PORT"
      "UAK.Auto.Snapshot Client$i 2"
      "UAK.Auto.Record Net.Client$i 300 30"
    )
    [[ -n "$ABILITY_CLASS" ]] && EXEC_CMDS+=("UAK.Auto.ActivateAbility $ABILITY_CLASS 3")
    EXEC="$(IFS='; '; echo "${EXEC_CMDS[*]}")"

    ARGS=(
      "$UPROJECT" -game
      "-ExecCmds=$EXEC"
      -unattended -nopause -nosplash
      "-abslog=$CLIENT_LOG"
    )
    if [[ "$HEADLESS" -eq 1 ]]; then
      ARGS+=(-NullRHI)
    else
      ARGS+=(-Windowed -ResX=640 -ResY=360)
    fi

    echo "[UAK] 启动：客户端 $i"
    "$EDITOR" "${ARGS[@]}" &
    CLIENT_PIDS+=($!)
    LOG_FILES+=("$CLIENT_LOG")
    sleep 3
  done

  echo "[UAK] 让多端跑 $NET_SECONDS 秒…"
  sleep "$NET_SECONDS"

  for pid in "${CLIENT_PIDS[@]}"; do kill "$pid" 2>/dev/null; done
  sleep 3
  kill "$SERVER_PID" 2>/dev/null
  echo "[UAK] Stage Net 结束"
fi

# ── 汇总报告 ────────────────────────────────────────────────────────────
GENERATOR="$SCRIPT_DIR/report/generate_report.py"
REPORT_EXIT=0
if command -v "$PYTHON_BIN" >/dev/null 2>&1; then
  PY_ARGS=("$GENERATOR" --project "$PROJECT_ROOT" --out docs/reports --stamp "$(date '+%Y-%m-%d %H:%M:%S')")
  for d in "${REPORT_DIRS[@]:-}"; do [[ -n "$d" ]] && PY_ARGS+=(--report-dir "$d"); done
  for l in "${LOG_FILES[@]:-}"; do [[ -n "$l" ]] && PY_ARGS+=(--log-file "$l"); done

  "$PYTHON_BIN" "${PY_ARGS[@]}"
  REPORT_EXIT=$?
else
  echo "[UAK] 未找到 $PYTHON_BIN，跳过报告生成（原始数据仍在 $BASE_DIR）" >&2
fi

echo "[UAK] 报告输出：$PROJECT_ROOT/docs/reports"

# ── 可选：提交结论（CI 只负责渲染 Pages）────────────────────────────────
if [[ "$DO_COMMIT" -eq 1 ]]; then
  (cd "$PROJECT_ROOT" \
    && git add docs/reports docs/showcase.md \
    && (git diff --staged --quiet || git commit -m "chore: update automation report ($STAMP)") \
    && { [[ "$DO_PUSH" -eq 1 ]] && git push || true; })
fi

exit "$REPORT_EXIT"
