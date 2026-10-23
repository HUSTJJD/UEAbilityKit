#!/usr/bin/env bash
# Thin entry point: forwards everything to Tools/lyra/cli.py.
# All Lyra logic lives in Python (Tools/lyra/*.py); this file only forwards args.
#
#   ./lyra.sh import --lyra-path <Lyra dir> --engine-root <Engine dir>
#   ./lyra.sh clean --keep-assets
#   ./lyra.sh patch --check
#   ./lyra.sh verify
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY="python3"; command -v "$PY" >/dev/null 2>&1 || PY="python"
exec "$PY" "$ROOT/lyra/cli.py" "$@"
