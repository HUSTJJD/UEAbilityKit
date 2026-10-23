# Thin entry point: forwards everything to Tools/tutorial/run.py.
#
#   ./generate_tutorials.ps1                      # record all + rebuild docs
#   ./generate_tutorials.ps1 --docs-only          # rebuild docs from existing media
#   ./generate_tutorials.ps1 --no-capture         # run steps without capturing
#   ./generate_tutorials.ps1 --tutorial 01-first-ability
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$py = 'python'
if (-not (Get-Command $py -ErrorAction SilentlyContinue)) { $py = 'python3' }
& $py (Join-Path $root 'tutorial/run.py') @args
exit $LASTEXITCODE
