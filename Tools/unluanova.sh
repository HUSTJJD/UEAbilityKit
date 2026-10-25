#!/usr/bin/env bash
# Thin entry point: forwards everything to Tools/unluanova/cli.py.
# All UnLuaNova integration logic lives in Python; this file only forwards args.
#
#   ./unluanova.sh setup
#   ./unluanova.sh doctor --strict
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
py="python3"
command -v "$py" >/dev/null 2>&1 || py="python"
exec "$py" "$root/unluanova/cli.py" "$@"
