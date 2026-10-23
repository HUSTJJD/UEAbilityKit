# Thin entry point: forwards everything to Tools/lyra/cli.py.
# All Lyra logic lives in Python (Tools/lyra/*.py); this file only forwards args.
#
#   ./lyra.ps1 import --lyra-path <Lyra dir> --engine-root <Engine dir>
#   ./lyra.ps1 clean --keep-assets
#   ./lyra.ps1 patch --check
#   ./lyra.ps1 verify
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$py = 'python'
if (-not (Get-Command $py -ErrorAction SilentlyContinue)) { $py = 'python3' }
& $py (Join-Path $root 'lyra/cli.py') @args
exit $LASTEXITCODE
