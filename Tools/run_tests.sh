#!/usr/bin/env bash
# Thin entry point: forwards everything to Tools/test/run.py.
#
#   ./run_tests.sh                                # all stages
#   ./run_tests.sh --stage editor --filter UAK.Unit
#   ./run_tests.sh --headless                     # no GPU: assertions only, no media
#   ./run_tests.sh --commit
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY="python3"; command -v "$PY" >/dev/null 2>&1 || PY="python"
exec "$PY" "$ROOT/test/run.py" "$@"
