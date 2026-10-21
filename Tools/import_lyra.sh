#!/usr/bin/env bash
#
# 把下载好的 Lyra 拷进本工程并打补丁（Linux / macOS），与 import_lyra.ps1 等价。
#
# 拷贝进来的 Lyra 代码保持 Lyra 命名（保留 Epic 版权头）；我们的东西用 AK 命名。
# 资产（约 1.4 GB）拷到本地但不进 git；源码与配置进 git。
#
# 用法：
#   ./import_lyra.sh -l ~/learning/LyraStarterGame
#   ./import_lyra.sh -l ~/learning/LyraStarterGame --skip-assets
#   ./import_lyra.sh -l ~/learning/LyraStarterGame --force
#
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

LYRA_PATH=""
ENGINE_ROOT="${UE_ENGINE_ROOT:-}"
SKIP_ASSETS=0
SKIP_PATCH=0
KEEP_OLD_DEMO=0
FORCE=0
PYTHON_BIN="${PYTHON:-python3}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -l|--lyra)        LYRA_PATH="$2"; shift 2 ;;
    -e|--engine-root) ENGINE_ROOT="$2"; shift 2 ;;
    --skip-assets)    SKIP_ASSETS=1; shift ;;
    --skip-patch)     SKIP_PATCH=1; shift ;;
    --keep-old-demo)  KEEP_OLD_DEMO=1; shift ;;
    --force)          FORCE=1; shift ;;
    -h|--help)        sed -n '2,14p' "$0"; exit 0 ;;
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

DEST_ROOT="$REPO_ROOT/Source/LyraGame"
mkdir -p "$DEST_ROOT"

# "完整可玩"需要的模块；排除 Settings / Hotfix / Performance / Replays / Development / Tests
LYRA_DIRS=(
  AbilitySystem Animation Audio Camera Character Cosmetics
  Equipment Feedback GameFeatures GameModes Input Interaction
  Inventory Messages Physics Player System Teams Weapons UI
)

MANIFEST=()

copy_dir() {
  local src="$1" dest="$2" label="$3"
  [[ -d "$src" ]] || { echo "[import] Lyra 里没有 $label，跳过"; return 0; }
  if [[ -d "$dest" && "$FORCE" -eq 0 ]]; then
    echo "[import] 已存在，跳过：$label（--force 可覆盖）"
    return 0
  fi
  cp -R "$src" "$dest"
  MANIFEST+=("$label")
  echo "[import] 源码：$label"
}

for f in "$LYRA_PATH"/Source/LyraGame/*; do
  [[ -f "$f" ]] || continue
  cp -f "$f" "$DEST_ROOT/"
done

for dir in "${LYRA_DIRS[@]}"; do
  copy_dir "$LYRA_PATH/Source/LyraGame/$dir" "$DEST_ROOT/$dir" "Source/LyraGame/$dir"
done

if [[ -d "$LYRA_PATH/Source/LyraEditor" ]]; then
  copy_dir "$LYRA_PATH/Source/LyraEditor" "$REPO_ROOT/Source/LyraEditor" "Source/LyraEditor"
fi

# 外围文件：copy（Lyra 是基线，我们的东西靠后面的 patch 加回去）
echo "[import] 拷外围文件（以 Lyra 为基线）…"

# .uproject：整个就是 Lyra 的（连名字一起），本工程 = 改造过后的 Lyra
OUR_UPROJECT="$REPO_ROOT/$(basename "$UPROJECT")"
if [[ -f "$OUR_UPROJECT" && "$FORCE" -eq 0 ]]; then cp "$OUR_UPROJECT" "$OUR_UPROJECT.uak.bak"; fi
cp "$UPROJECT" "$OUR_UPROJECT"
echo "[import] .uproject：$(basename "$UPROJECT") ← Lyra"

# 旧的 UAKDemo 工程文件不再需要
if [[ "$KEEP_OLD_DEMO" -eq 0 ]]; then
  [[ -f "$REPO_ROOT/UAKDemo.uproject" ]] && { rm -f "$REPO_ROOT/UAKDemo.uproject"; echo "[import] 删除 UAKDemo.uproject"; }
  [[ -d "$REPO_ROOT/Source/UAKDemo" ]] && { rm -rf "$REPO_ROOT/Source/UAKDemo"; echo "[import] 删除 Source/UAKDemo"; }
  for t in "$REPO_ROOT"/Source/UAKDemo*.Target.cs; do
    [[ -f "$t" ]] || continue
    rm -f "$t"; echo "[import] 删除 $(basename "$t")"
  done
fi

mkdir -p "$REPO_ROOT/Config"
for ini in "$LYRA_PATH"/Config/*.ini; do
  [[ -f "$ini" ]] || continue
  name="$(basename "$ini")"
  target="$REPO_ROOT/Config/$name"
  if [[ -f "$target" && "$FORCE" -eq 0 ]]; then cp "$target" "$target.uak.bak"; fi
  cp "$ini" "$target"
done
echo "[import] Config ← Lyra"

for t in "$LYRA_PATH"/Source/*.Target.cs; do
  [[ -f "$t" ]] || continue
  name="$(basename "$t")"
  target="$REPO_ROOT/Source/$name"
  if [[ -f "$target" && "$FORCE" -eq 0 ]]; then cp "$target" "$target.uak.bak"; fi
  cp "$t" "$target"
done
echo "[import] Target.cs ← Lyra"

if [[ "$SKIP_ASSETS" -eq 0 ]]; then
  echo "[import] 拷资产（约 1.4 GB，不进 git）…"
  mkdir -p "$REPO_ROOT/Plugins"

  if [[ -d "$LYRA_PATH/Plugins/GameFeatures" ]]; then
    mkdir -p "$REPO_ROOT/Plugins/GameFeatures"
    for gf in "$LYRA_PATH"/Plugins/GameFeatures/*; do
      [[ -d "$gf" ]] || continue
      name="$(basename "$gf")"
      if [[ -d "$REPO_ROOT/Plugins/GameFeatures/$name" && "$FORCE" -eq 0 ]]; then
        continue
      fi
      cp -R "$gf" "$REPO_ROOT/Plugins/GameFeatures/"
      echo "[import] 资产插件：GameFeatures/$name"
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
      if [[ -d "$REPO_ROOT/Plugins/$name" && "$FORCE" -eq 0 ]]; then continue; fi
      cp -R "$p" "$REPO_ROOT/Plugins/"
      echo "[import] Lyra 插件：$name"
    done
  else
    echo "[import] 未指定引擎目录，跳过 Lyra 自带插件筛选" >&2
  fi

  if [[ -d "$LYRA_PATH/Platforms" ]]; then
    cp -R "$LYRA_PATH/Platforms" "$REPO_ROOT/"
    echo "[import] 平台配置：Platforms/"
  fi
fi

if [[ "$SKIP_PATCH" -eq 0 ]]; then
  if command -v "$PYTHON_BIN" >/dev/null 2>&1; then
    "$PYTHON_BIN" "$SCRIPT_DIR/patch_lyra.py" || exit 1
  else
    echo "[import] 警告：没有 $PYTHON_BIN，请手动让 ULyraGameplayAbility 继承 UAKGameplayAbility" >&2
  fi
fi

printf '%s\n' "${MANIFEST[@]}" > "$SCRIPT_DIR/lyra_manifest.txt"
echo "[import] 来源清单：$SCRIPT_DIR/lyra_manifest.txt"
echo ""
echo "[import] 完成。下一步：编译 → 打开工程 → 用 UAK 资产重写 Lyra 的技能"
