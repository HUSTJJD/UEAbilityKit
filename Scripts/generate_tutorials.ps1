<#
.SYNOPSIS
    UAK 一键教程流水线：录制编辑器操作过程 → 产出截图 / 录屏 → 生成教程文档。

.DESCRIPTION
    教程不是手写的。每一步的讲解与"要做什么"写在 Tutorials/*.json 里，
    由编辑器的 `UAK.Tutorial.RunAll` 真实执行一遍并采集素材，
    再由 Scripts/tutorial/generate_tutorials.py 渲染成 docs/tutorials/*.md。

    与 run_tests.ps1 一样：**只在本地跑**（要有 UE、要有真实 RHI 才有像素），
    CI 只负责把生成的文档渲染到 GitHub Pages。

    注意这里用的是 UnrealEditor.exe（完整编辑器）而不是 -Cmd：
    教程要截的是资产编辑器面板，需要真实的 Slate 窗口与渲染资源。

.EXAMPLE
    ./Scripts/generate_tutorials.ps1                 # 录制全部教程并生成文档
    ./Scripts/generate_tutorials.ps1 -DocsOnly       # 只用已有产物重新生成文档
    ./Scripts/generate_tutorials.ps1 -NoCapture      # 只验证步骤能不能跑通，不采素材
    ./Scripts/generate_tutorials.ps1 -Tutorial 01-first-ability
#>
[CmdletBinding()]
param(
    [string] $Project        = '',
    [string] $EngineRoot     = $env:UE_ENGINE_ROOT,
    [string] $Python         = 'python',
    [switch] $DocsOnly,          # 不启动 UE，只用已有产物
    [switch] $NoCapture,         # 跑步骤但不截图 / 不录屏
    [string] $Tutorial      = '',  # 只跑某一条（教程 id）；留空跑全部
    [int]    $TimeoutSeconds = 1200,
    [switch] $Commit
)

$ErrorActionPreference = 'Stop'

# ── 路径 ────────────────────────────────────────────────────────────────
if ([string]::IsNullOrWhiteSpace($Project)) {
    $Project = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
}
$Project = (Resolve-Path $Project).Path

$scriptDir = Join-Path $Project 'Tutorials'
if (-not (Test-Path $scriptDir)) { throw "找不到教程脚本目录：$scriptDir" }

$generator = Join-Path $PSScriptRoot 'tutorial\generate_tutorials.py'

function Invoke-Generator {
    $args = @($generator, '--project', $Project)
    Write-Host "[UAK] 生成教程文档…" -ForegroundColor Cyan
    & $Python @args
    if ($LASTEXITCODE -ne 0) { throw "生成教程文档失败（退出码 $LASTEXITCODE）" }
}

# ── 只生成文档 ──────────────────────────────────────────────────────────
if ($DocsOnly) {
    Invoke-Generator
    return
}

# ── 录制 ────────────────────────────────────────────────────────────────
$uproject = Join-Path $Project 'LyraStarterGame.uproject'
if (-not (Test-Path $uproject)) {
    $found = Get-ChildItem -Path $Project -Filter '*.uproject' -File | Select-Object -First 1
    if (-not $found) { throw "找不到工程文件：$Project 下没有 .uproject，请先跑 Tools\import_lyra.ps1" }
    $uproject = $found.FullName
}

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $assoc = (Get-Content $uproject -Raw | ConvertFrom-Json).EngineAssociation
    if ($assoc -match '^\{[0-9A-Fa-f-]+\}$') {
        $key = "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds"
        $EngineRoot = (Get-ItemProperty -Path $key -Name $assoc -ErrorAction SilentlyContinue).$assoc
    }
}
if ([string]::IsNullOrWhiteSpace($EngineRoot) -or -not (Test-Path $EngineRoot)) {
    throw "找不到 UE 引擎：请用 -EngineRoot 指定，或设置环境变量 UE_ENGINE_ROOT"
}

$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path $editor)) { throw "找不到编辑器：$editor" }

$cmds = @()
if ($NoCapture) { $cmds += 'UAK.Tutorial.Capture 0' }
if ([string]::IsNullOrWhiteSpace($Tutorial)) {
    $cmds += 'UAK.Tutorial.RunAll -Quit'
} else {
    $cmds += "UAK.Tutorial.Run $Tutorial"
}
$execCmds = $cmds -join '; '

Write-Host "[UAK] 启动编辑器录制教程：$execCmds" -ForegroundColor Cyan
$proc = Start-Process -FilePath $editor -ArgumentList @(
    "`"$uproject`"",
    "-ExecCmds=`"$execCmds`"",
    '-unattended', '-nosplash', '-stdout', '-FullStdOutLogOutput'
) -PassThru -NoNewWindow

try {
    $proc | Wait-Process -Timeout $TimeoutSeconds -ErrorAction Stop
} catch {
    Write-Warning "[UAK] 超时（${TimeoutSeconds}s），强制结束编辑器；已产出的步骤仍会写进 run.json"
    if (-not $proc.HasExited) { $proc.Kill() }
}

Invoke-Generator

if ($Commit) {
    Push-Location $Project
    try {
        git add docs/tutorials Tutorials mkdocs.yml
        git commit -m "docs(tutorials): 重新录制并生成编辑器教程"
        Write-Host "[UAK] 已提交；CI 会把它渲染到 Pages" -ForegroundColor Green
    } finally {
        Pop-Location
    }
}
