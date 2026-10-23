#!/usr/bin/env bash
# Thin entry point: forwards everything to Tools/build/build.py.
#
#   ./build.sh                                    # <Project>Editor / Win64 / Development
#   ./build.sh --target LyraGame --configuration Shipping
#   ./build.sh --clean
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY="python3"; command -v "$PY" >/dev/null 2>&1 || PY="python"
exec "$PY" "$ROOT/build/build.py" "$@"
