#!/usr/bin/env bash
#
# 一键清除本工程里所有从 Lyra 拷进来的内容，回到"还没导入 Lyra"的干净状态（与 clean_lyra.ps1 等价）。
#
# 为什么要清：拷进来的 Lyra 一旦被手改过，测试就会"因为改过而通过"，把 bug 藏起来。
# 清完之后工程 = Lyra 原版 + Tools/lyra_patches.json 声明的补丁，别的什么都不许有。
#
# 用法：
#   ./clean_lyra.sh                 # 清源码 + 配置 + 资产
#   ./clean_lyra.sh --keep-assets   # 只清源码/配置，1.4 GB 资产留着
#   ./clean_lyra.sh --deep          # 连 Binaries/Intermediate/DerivedDataCache/Saved 一起清
#   ./clean_lyra.sh --dry-run       # 只列出来，不真删
#
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

KEEP_ASSETS=0
DEEP=0
DRY_RUN=0
KEEP_PLUGINS=(UEAbilityKit UnLuaNova)

while [[ $# -gt 0 ]]; do
  case "$1" in
    --keep-assets)  KEEP_ASSETS=1; shift ;;
    --deep)         DEEP=1; shift ;;
    --dry-run)      DRY_RUN=1; shift ;;
    --keep-plugin)  KEEP_PLUGINS+=("$2"); shift 2 ;;
    -h|--help)      sed -n '2,16p' "$0"; exit 0 ;;
    *) echo "未知参数：$1" >&2; exit 2 ;;
  esac
done

REMOVED=0

rm_target() {
  local rel="$1" full
  full="$REPO_ROOT/$rel"
  # 防御：绝不删仓库外的东西
  case "$full" in
    "$REPO_ROOT"/*) ;;
    *) echo "[clean] 拒绝删除仓库之外的路径：$full" >&2; return 1 ;;
  esac
  [[ -e "$full" ]] || return 0
  if [[ "$DRY_RUN" -eq 1 ]]; then
    echo "[clean] 将删除：$rel"
  else
    rm -rf "$full"
    echo "[clean] 删除：$rel"
  fi
  REMOVED=$((REMOVED + 1))
}

echo "[clean] 工程根目录：$REPO_ROOT"

# 1. 上次导入的清单（照单全删，最准）
# --keep-assets 时 Plugins/ 下的条目同样要留着，否则清单会把资产一个个删光
if [[ -f "$SCRIPT_DIR/.lyra_paths.txt" ]]; then
  while IFS= read -r line; do
    [[ -n "${line// /}" ]] || continue
    if [[ "$KEEP_ASSETS" -eq 1 && ( "$line" == Plugins/* || "$line" == Content/Shooter* ) ]]; then
      continue
    fi
    rm_target "$line"
  done < "$SCRIPT_DIR/.lyra_paths.txt"
fi

# 2. 固定清单（清单丢了也能清干净）
for rel in Source/LyraGame Source/LyraEditor Config Platforms \
           Content/ShooterCore Content/ShooterMaps; do
  rm_target "$rel"
done

for f in "$REPO_ROOT"/Source/*.Target.cs; do
  [[ -f "$f" ]] && rm_target "Source/$(basename "$f")"
done
for f in "$REPO_ROOT"/*.uproject; do
  [[ -f "$f" ]] && rm_target "$(basename "$f")"   # 根目录 .uproject 整份都是 Lyra 的
done

# 3. 资产：Plugins/ 下除了我们自己的插件，其余都是 Lyra 拷来的
if [[ "$KEEP_ASSETS" -eq 0 ]]; then
  for p in "$REPO_ROOT"/Plugins/*; do
    [[ -d "$p" ]] || continue
    name="$(basename "$p")"
    keep=0
    for k in "${KEEP_PLUGINS[@]}"; do [[ "$k" == "$name" ]] && keep=1; done
    if [[ "$keep" -eq 1 ]]; then
      echo "[clean] 保留（我们的）：Plugins/$name"
    else
      rm_target "Plugins/$name"
    fi
  done
else
  echo "[clean] --keep-assets：保留资产插件（资产不会重新校验，只是省时间）"
fi

# 4. 补丁痕迹
while IFS= read -r junk; do
  [[ -n "$junk" ]] && rm_target "${junk#$REPO_ROOT/}"
done < <(find "$REPO_ROOT" -type f \( -name '*.orig' -o -name '*.uak.bak' \) 2>/dev/null)

for rel in Tools/.patched_files.txt Tools/.lyra_state.json Tools/.lyra_paths.txt Tools/lyra_manifest.txt; do
  rm_target "$rel"
done

# 5. 引擎生成物（可选）
if [[ "$DEEP" -eq 1 ]]; then
  for rel in Binaries Intermediate DerivedDataCache Saved; do rm_target "$rel"; done
  echo "[clean] --deep：已清引擎生成物（下次编译是全量，慢但最干净）"
fi

echo ""
if [[ "$DRY_RUN" -eq 1 ]]; then
  echo "[clean] 预演完成：共 $REMOVED 项将被删除（未改动任何文件）"
else
  echo "[clean] 完成：删除 $REMOVED 项，工程已回到未导入 Lyra 的状态"
  echo "[clean] 下一步：./import_lyra.sh -l <你的 Lyra 目录>"
fi
