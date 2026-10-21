#!/usr/bin/env bash
#
# 把下载好的 Lyra 拷进本工程并打补丁（Linux / macOS），与 import_lyra.ps1 等价。
#
# 流程：先清（clean_lyra.sh）→ 拷源码/外围/资产 → 打补丁 → 记账 → 校验。
# 铁律：不改 Lyra 的内容；必须改的写在 Tools/lyra_patches.json 里，由 patch_lyra.py 自动打。
# 拷进来的 Lyra 代码保持 Lyra 命名（保留 Epic 版权头）；我们的东西用 AK 命名。
# 资产（约 1.4 GB）拷到本地但不进 git；源码与配置进 git。
#
# 用法：
#   ./import_lyra.sh -l <你的 Lyra 目录>
#   ./import_lyra.sh -l <你的 Lyra 目录> --skip-assets
#   ./import_lyra.sh -l <你的 Lyra 目录> --keep-assets   # 资产沿用上次，只重拷源码
#   ./import_lyra.sh -l <你的 Lyra 目录> --no-clean      # 不预清（不推荐，只调试用）
#
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

LYRA_PATH=""
ENGINE_ROOT="${UE_ENGINE_ROOT:-}"
SKIP_ASSETS=0
SKIP_PATCH=0
KEEP_ASSETS=0
NO_CLEAN=0
PYTHON_BIN="${PYTHON:-python3}"

IMPORTED=()   # 记下来给 clean_lyra 用
MANIFEST=()   # 给人看的来源清单

while [[ $# -gt 0 ]]; do
  case "$1" in
    -l|--lyra)        LYRA_PATH="$2"; shift 2 ;;
    -e|--engine-root) ENGINE_ROOT="$2"; shift 2 ;;
    --skip-assets)    SKIP_ASSETS=1; shift ;;
    --skip-patch)     SKIP_PATCH=1; shift ;;
    --keep-assets)    KEEP_ASSETS=1; shift ;;
    --no-clean)       NO_CLEAN=1; shift ;;
    --force)          shift ;;   # 兼容旧用法：现在默认就是"清完重拷"
    -h|--help)        sed -n '2,19p' "$0"; exit 0 ;;
    *) echo "未知参数：$1" >&2; exit 2 ;;
  esac
done

[[ -n "$LYRA_PATH" ]] || { echo "请用 -l 指定下载好的 Lyra 工程目录" >&2; exit 1; }
[[ -d "$LYRA_PATH" ]] || { echo "Lyra 目录不存在：$LYRA_PATH" >&2; exit 1; }
LYRA_PATH="$(cd "$LYRA_PATH" && pwd)"

UPROJECT=""
for name in LyraStarterGame.uproject Lyra.uproject; do
  [[ -f "$LYRA_PATH/$name" ]] && { UPROJECT="$LYRA_PATH/$name"; break; }
done
[[ -n "$UPROJECT" ]] || UPROJECT="$(find "$LYRA_PATH" -maxdepth 1 -name '*.uproject' | head -n 1)"
[[ -n "$UPROJECT" ]] || { echo "$LYRA_PATH 下找不到 .uproject" >&2; exit 1; }

echo "[import] Lyra：$UPROJECT"

# 引擎关联：本机源码引擎的 GUID 写进"本机覆盖"，绝不写进仓库里的 lyra_patches.json
# （Linux / macOS 没有注册表，用环境变量 UE_ENGINE_GUID 指定）
if [[ -n "$ENGINE_ROOT" && -n "${UE_ENGINE_GUID:-}" ]]; then
  cat > "$SCRIPT_DIR/lyra_patches.local.json" <<EOF
{
  "comment": "由 import_lyra 自动生成：本机引擎的 GUID。不进仓库。",
  "uproject": { "set_engine_association": "$UE_ENGINE_GUID" }
}
EOF
  echo "[import] 引擎关联：$UE_ENGINE_GUID（写进 lyra_patches.local.json，不进仓库）"
fi

# 拷 + 记账（clean_lyra 按这份清单删）
copy_lyra() {
  local src="$1" rel="$2"
  mkdir -p "$REPO_ROOT/$(dirname "$rel")"
  cp -R "$src" "$REPO_ROOT/$rel"
  IMPORTED+=("$rel")
}

# ── 0. 先清干净：没有这一步，上一次的手改会留下来骗过测试 ────────────────
if [[ "$NO_CLEAN" -eq 0 ]]; then
  echo "[import] 预清理（保证这次导入是唯一的变量来源）…"
  if [[ "$KEEP_ASSETS" -eq 1 ]]; then
    "$SCRIPT_DIR/clean_lyra.sh" --keep-assets || exit 1
  else
    "$SCRIPT_DIR/clean_lyra.sh" || exit 1
  fi
else
  echo "[import] --no-clean：跳过预清理，工程里可能残留上次的手改（只用于调试）" >&2
fi

# ── 1. 拷源码 ────────────────────────────────────────────────────────────
# 排除：Settings / Hotfix / Performance / Replays / Development / Tests
LYRA_DIRS=(
  AbilitySystem Animation Audio Camera Character Cosmetics
  Equipment Feedback GameFeatures GameModes Input Interaction
  Inventory Messages Physics Player System Teams Weapons UI
)

for f in "$LYRA_PATH"/Source/LyraGame/*; do
  [[ -f "$f" ]] || continue
  copy_lyra "$f" "Source/LyraGame/$(basename "$f")"
done

for dir in "${LYRA_DIRS[@]}"; do
  src="$LYRA_PATH/Source/LyraGame/$dir"
  [[ -d "$src" ]] || { echo "[import] Lyra 里没有 $dir，跳过"; continue; }
  copy_lyra "$src" "Source/LyraGame/$dir"
  MANIFEST+=("Source/LyraGame/$dir")
  echo "[import] 源码：$dir"
done

if [[ -d "$LYRA_PATH/Source/LyraEditor" ]]; then
  copy_lyra "$LYRA_PATH/Source/LyraEditor" "Source/LyraEditor"
  MANIFEST+=("Source/LyraEditor")
  echo "[import] 源码：LyraEditor"
fi

# ── 2. 外围文件：copy（Lyra 是基线，我们的东西靠后面的 patch 加回去）──────
echo "[import] 拷外围文件（以 Lyra 为基线）…"

# 2a. .uproject：整个就是 Lyra 的（连名字一起）
UPROJECT_NAME="$(basename "$UPROJECT")"
copy_lyra "$UPROJECT" "$UPROJECT_NAME"
echo "[import] .uproject：$UPROJECT_NAME ← Lyra"

# 说明：UAKDemo 是插件的一个模块（Plugins/UEAbilityKit/Source/UAKDemo），
# 不再是独立工程，所以这里不需要删任何东西。

mkdir -p "$REPO_ROOT/Config"
for ini in "$LYRA_PATH"/Config/*.ini; do
  [[ -f "$ini" ]] || continue
  copy_lyra "$ini" "Config/$(basename "$ini")"
done
echo "[import] Config ← Lyra"

for t in "$LYRA_PATH"/Source/*.Target.cs; do
  [[ -f "$t" ]] || continue
  copy_lyra "$t" "Source/$(basename "$t")"
done
echo "[import] Target.cs ← Lyra"

# ── 3. 拷资产（大，不进 git）──────────────────────────────────────────────
if [[ "$SKIP_ASSETS" -eq 0 ]]; then
  echo "[import] 拷资产（约 1.4 GB，不进 git）…"
  mkdir -p "$REPO_ROOT/Plugins"

  if [[ -d "$LYRA_PATH/Plugins/GameFeatures" ]]; then
    for gf in "$LYRA_PATH"/Plugins/GameFeatures/*; do
      [[ -d "$gf" ]] || continue
      copy_lyra "$gf" "Plugins/GameFeatures/$(basename "$gf")"
      echo "[import] 资产插件：GameFeatures/$(basename "$gf")"
    done
  fi

  if [[ -n "$ENGINE_ROOT" && -d "$ENGINE_ROOT/Engine/Plugins" ]]; then
    for p in "$LYRA_PATH"/Plugins/*; do
      [[ -d "$p" ]] || continue
      name="$(basename "$p")"
      [[ "$name" == "GameFeatures" ]] && continue
      if [[ -d "$ENGINE_ROOT/Engine/Plugins/$name" ]]; then
        echo "[import] 引擎自带，跳过：$name"
        continue
      fi
      copy_lyra "$p" "Plugins/$name"
      echo "[import] Lyra 插件：$name"
    done
  else
    # 没给引擎目录就别自作聪明地跳过：Lyra 的插件缺一个就编译不过，宁可全拷
    echo "[import] 未指定引擎目录：Lyra 自带插件不做筛选，全部拷进来（可能与引擎自带插件重名）" >&2
    for p in "$LYRA_PATH"/Plugins/*; do
      [[ -d "$p" ]] || continue
      name="$(basename "$p")"
      [[ "$name" == "GameFeatures" ]] && continue
      copy_lyra "$p" "Plugins/$name"
      echo "[import] Lyra 插件：$name"
    done
  fi
fi

# 平台配置（小，属于外围文件，跟 --skip-assets 无关）
if [[ -d "$LYRA_PATH/Platforms" ]]; then
  copy_lyra "$LYRA_PATH/Platforms" "Platforms"
  echo "[import] 平台配置：Platforms/"
fi

# ── 4. 打补丁：唯一允许改动 Lyra 的地方 ────────────────────────────────────
if [[ "$SKIP_PATCH" -eq 0 ]]; then
  if command -v "$PYTHON_BIN" >/dev/null 2>&1; then
    "$PYTHON_BIN" "$SCRIPT_DIR/patch_lyra.py" || exit 1
  else
    echo "[import] 警告：没有 $PYTHON_BIN，请手动让 ULyraGameplayAbility 继承 UAKGameplayAbility" >&2
  fi
fi

# ── 5. 记账 ───────────────────────────────────────────────────────────────
if ((${#IMPORTED[@]})); then
  printf '%s\n' "${IMPORTED[@]}" | sort -u > "$SCRIPT_DIR/.lyra_paths.txt"
else
  : > "$SCRIPT_DIR/.lyra_paths.txt"
fi
echo "[import] 导入清单（clean_lyra 按它删）：$SCRIPT_DIR/.lyra_paths.txt"

if ((${#MANIFEST[@]})); then
  printf '%s\n' "${MANIFEST[@]}" | sort -u > "$SCRIPT_DIR/lyra_manifest.txt"
else
  : > "$SCRIPT_DIR/lyra_manifest.txt"
fi
echo "[import] 来源清单：$SCRIPT_DIR/lyra_manifest.txt"

# ── 6. 校验：确认工程 = Lyra 原版 + 声明过的补丁 ──────────────────────────
if command -v "$PYTHON_BIN" >/dev/null 2>&1; then
  "$PYTHON_BIN" "$SCRIPT_DIR/verify_lyra.py" -l "$LYRA_PATH" --update-state \
    || echo "[import] 校验没通过：上面列出的文件与 Lyra 原版不一致" >&2
fi

echo ""
echo "[import] 完成。下一步：编译 → 打开工程 → 用 UAK 资产重写 Lyra 的技能"
echo "[import] 随时回到干净状态：$SCRIPT_DIR/clean_lyra.sh（然后重跑本脚本即可复现）"
