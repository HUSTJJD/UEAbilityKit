<#
.SYNOPSIS
    UAK 测试启动脚本：编译 → 跑 UE Automation → 生成报告 / 截图 / 录屏 → 更新 docs。

.DESCRIPTION
    一条命令覆盖"跑测试 + 出结论 + 出素材 + 出文档"：
      1. 用 UBT 编译 UAKDemoEditor（可 -SkipBuild 跳过）
      2. 用 UnrealEditor-Cmd 跑 Automation（默认 -NullRHI，headless 可跑）
      3. 调 Scripts/report/generate_report.py 生成
         docs/reports/latest.{md,html,json} 与 docs/showcase.md
      4. 有 ffmpeg 且开了 -CaptureMedia 时，把帧序列合成 mp4

.EXAMPLE
    ./Scripts/run_tests.ps1                      # 全量跑
    ./Scripts/run_tests.ps1 -Filter UAK.Unit     # 只跑单元测试
    ./Scripts/run_tests.ps1 -SkipBuild           # 已编译过，只跑测试
    ./Scripts/run_tests.ps1 -CaptureMedia        # 带截图与录屏（需要真实 RHI）
#>
[CmdletBinding()]
param(
    [string] $Project       = '',
    [string] $EngineRoot    = $env:UE_ENGINE_ROOT,
    [string] $Filter        = 'UAK',
    [switch] $SkipBuild,
    [switch] $CaptureMedia,
    [string] $Python        = 'python',
    [int]    $TimeoutSeconds = 2400
)

$ErrorActionPreference = 'Stop'

# ── 路径 ────────────────────────────────────────────────────────────────
if ([string]::IsNullOrWhiteSpace($Project)) {
    $Project = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
}
$Project = (Resolve-Path $Project).Path
$uproject = Join-Path $Project 'UAKDemo.uproject'
if (-not (Test-Path $uproject)) { throw "找不到工程文件：$uproject" }

# 引擎路径：显式指定 > 注册表按 .uproject 的 EngineAssociation 反查
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

$ubt     = Join-Path $EngineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'
$editor  = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$srcDir  = Join-Path $EngineRoot 'Engine\Source'
foreach ($f in @($ubt, $editor, $srcDir)) {
    if (-not (Test-Path $f)) { throw "引擎目录不完整，缺少：$f" }
}

$stamp     = Get-Date -Format 'yyyyMMdd-HHmmss'
$reportDir = Join-Path $Project "Saved\Automation\Reports\$stamp"
$logDir    = Join-Path $Project "Saved\Automation\Logs"
$logFile   = Join-Path $logDir "Automation-$stamp.log"
New-Item -ItemType Directory -Force -Path $reportDir, $logDir | Out-Null

Write-Host "[UAK] 工程：$Project"
Write-Host "[UAK] 引擎：$EngineRoot"
Write-Host "[UAK] 过滤：$Filter"

# ── 1. 编译 ─────────────────────────────────────────────────────────────
if (-not $SkipBuild) {
    Write-Host "[UAK] 编译 UAKDemoEditor (Win64/Development)…"
    Push-Location $srcDir
    try {
        & dotnet $ubt UAKDemoEditor Win64 Development "-Project=$uproject" -WaitMutex
        if ($LASTEXITCODE -ne 0) { throw "编译失败（exit=$LASTEXITCODE）" }
    }
    finally { Pop-Location }
    Write-Host "[UAK] 编译完成"
}

# ── 2. 跑自动化 ─────────────────────────────────────────────────────────
$editorArgs = @(
    "`"$uproject`"",
    "-ExecCmds=`"Automation RunTests $Filter; Quit`"",
    "-TestExit=`"Automation Test Queue Empty`"",
    "-ReportExportPath=`"$reportDir`"",
    "-unattended", "-nopause", "-nosplash",
    "-abslog=`"$logFile`""
)
if ($CaptureMedia) {
    # 需要真实 RHI 才有像素；同时告诉测试模块开启截图 / 帧录制
    $editorArgs += @('-Windowed', '-ResX=1280', '-ResY=720', '-UAKCaptureMedia=1')
}
else {
    $editorArgs += '-NullRHI'
}

Write-Host "[UAK] 运行自动化：$Filter"
$proc = Start-Process -FilePath $editor -ArgumentList $editorArgs -PassThru -WindowStyle Minimized
$exited = $proc.WaitForExit($TimeoutSeconds * 1000)
if (-not $exited) {
    Write-Warning "[UAK] 超时 ${TimeoutSeconds}s，强制结束编辑器"
    $proc.Kill()
}
Write-Host "[UAK] 编辑器退出码：$($proc.ExitCode)"

# ── 3. 生成报告 ─────────────────────────────────────────────────────────
$generator = Join-Path $PSScriptRoot 'report\generate_report.py'
if (Get-Command $Python -ErrorAction SilentlyContinue) {
    & $Python $generator `
        --project $Project `
        --report-dir $reportDir `
        --log-file $logFile `
        --out 'docs/reports' `
        --stamp (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
    $reportExit = $LASTEXITCODE
}
else {
    Write-Warning "[UAK] 未找到 python，跳过报告生成（原始数据仍在 $reportDir）"
    $reportExit = 0
}

Write-Host "[UAK] 报告输出：$(Join-Path $Project 'docs\reports')"
exit $reportExit
