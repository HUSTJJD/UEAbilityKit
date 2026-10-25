# Thin entry point: forwards everything to Tools/unluanova/cli.py.
# All UnLuaNova integration logic lives in Python (Tools/unluanova/*.py); this file only forwards args.
#
#   ./unluanova.ps1 setup
#   ./unluanova.ps1 setup --backend Lua --disable-extensions
#   ./unluanova.ps1 doctor
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$py = 'python'
if (-not (Get-Command $py -ErrorAction SilentlyContinue)) { $py = 'python3' }
& $py (Join-Path $root 'unluanova/cli.py') @args
exit $LASTEXITCODE
