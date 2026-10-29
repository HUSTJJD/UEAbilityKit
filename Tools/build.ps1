# Thin entry point: forwards everything to Tools/build/build.py.
#
#   ./build.ps1                                   # <Project>Editor / Win64 / Development
#   ./build.ps1 --target UAKSample --configuration Shipping
#   ./build.ps1 --clean
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$py = 'python'
if (-not (Get-Command $py -ErrorAction SilentlyContinue)) { $py = 'python3' }
& $py (Join-Path $root 'build/build.py') @args
exit $LASTEXITCODE
