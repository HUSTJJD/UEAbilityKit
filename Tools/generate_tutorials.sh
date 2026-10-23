#!/usr/bin/env bash
# Thin entry point: forwards everything to Tools/tutorial/run.py.
#
#   ./generate_tutorials.sh                       # record all + rebuild docs
#   ./generate_tutorials.sh --docs-only           # rebuild docs from existing media
#   ./generate_tutorials.sh --no-capture          # run steps without capturing
#   ./generate_tutorials.sh --tutorial 01-first-ability
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY="python3"; command -v "$PY" >/dev/null 2>&1 || PY="python"
exec "$PY" "$ROOT/tutorial/run.py" "$@"
