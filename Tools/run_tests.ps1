# Thin entry point: forwards everything to Tools/test/run.py.
#
#   ./run_tests.ps1                               # all stages
#   ./run_tests.ps1 --stage editor --filter UAK.Unit
#   ./run_tests.ps1 --headless                    # no GPU: assertions only, no media
#   ./run_tests.ps1 --commit
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$py = 'python'
if (-not (Get-Command $py -ErrorAction SilentlyContinue)) { $py = 'python3' }
& $py (Join-Path $root 'test/run.py') @args
exit $LASTEXITCODE
