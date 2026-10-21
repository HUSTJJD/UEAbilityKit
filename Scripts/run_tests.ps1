<#
.SYNOPSIS
    UAK 一键测试：本地编译 → 跑三个 stage → 生成报告 / 截图 / 录屏 → 可选提交。

.DESCRIPTION
    测试**只在本地跑**（有 GPU、有 UE）。CI 不跑 UE，只把本脚本产出的
    docs/reports 与 docs/showcase.md 渲染到 GitHub Pages。

    三个 stage：
      1. Editor   编辑器内 Automation（UAK.*）：单元 / 实例 / 编辑器 UI，逐帧截图
      2. Game     独立进程 -game：真实游戏世界里的技能表现，逐帧录屏
      3. Net      DS + N 个客户端：各端导出 GAS 快照，脚本比对属性是否收敛

    stage 2 / 3 不用 Automation 用例，而用 Runtime 模块的 `UAK.Auto.*` 控制台命令
    ——因为测试模块是 Editor 类型，在 -game / -server 进程里不一定加载。

.EXAMPLE
    ./Scripts/run_tests.ps1                          # 全部 stage（默认开采集）
    ./Scripts/run_tests.ps1 -Stage Editor -Filter UAK.Unit
    ./Scripts/run_tests.ps1 -Headless                # 没 GPU：只跑断言，不出素材
    ./Scripts/run_tests.ps1 -Stage Net -Clients 3 -AbilityClass /Script/UAKDemo.UAKDemoAbility
    ./Scripts/run_tests.ps1 -Commit                  # 跑完把结论提交（CI 负责发布）
#>
[CmdletBinding()]
param(
    [string] $Project         = '',
    [string] $EngineRoot      = $env:UE_ENGINE_ROOT,
    [ValidateSet('All', 'Editor', 'Game', 'Net')]
    [string] $Stage           = 'All',
    [switch] $Headless,          # -NullRHI：没有 GPU / 远程桌面时用，不产素材
    [switch] $SkipBuild,
    [string] $Filter         = 'UAK',
    [int]    $Clients        = 2,
    [int]    $Port           = 7777,
    [string] $Map            = '',       # 留空用工程的 GameDefaultMap
    [string] $AbilityClass   = '',       # 例：/Script/UAKDemo.UAKDemoAbility
    [int]    $GameSeconds    = 12,       # stage 2 让进程跑多久（秒）
    [int]    $NetSeconds     = 25,       # stage 3 客户端跑多久（秒）
    [int]    $ServerBootWait = 10,       # 等 DS 起来（秒）
    [string] $Python         = 'python',
    [int]    $TimeoutSeconds = 2400,
    [switch] $Commit,
    [switch] $Push
)

$ErrorActionPreference = 'Stop'

# ── 路径 ────────────────────────────────────────────────────────────────
if ([string]::IsNullOrWhiteSpace($Project)) {
    $Project = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
}
$Project = (Resolve-Path $Project).Path
$uproject = Join-Path $Project 'UAKDemo.uproject'
if (-not (Test-Path $uproject)) { throw "找不到工程文件：$uproject" }

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

$ubt    = Join-Path $EngineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$srcDir = Join-Path $EngineRoot 'Engine\Source'
foreach ($f in @($ubt, $editor, $srcDir)) {
    if (-not (Test-Path $f)) { throw "引擎目录不完整，缺少：$f" }
}

$stamp     = Get-Date -Format 'yyyyMMdd-HHmmss'
$baseDir   = Join-Path $Project "Saved\Automation"
$reportDir = Join-Path $baseDir "Reports\$stamp"
$logDir    = Join-Path $baseDir 'Logs'
New-Item -ItemType Directory -Force -Path $reportDir, $logDir | Out-Null

$reportDirs = @()
$logFiles   = @()

Write-Host "[UAK] 工程：$Project"
Write-Host "[UAK] 引擎：$EngineRoot"
Write-Host "[UAK] stage：$Stage　（headless=$($Headless.IsPresent)）"
if (-not $Headless -and -not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
    Write-Warning "[UAK] 没找到 ffmpeg：帧序列不会合成 mp4（截图仍会产出）"
}

# ── 编译 ────────────────────────────────────────────────────────────────
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

# ── 通用：起一个进程，最多等 N 秒，超时就杀 ──────────────────────────────
function Invoke-UakProcess {
    param(
        [string] $Exe,
        [string[]] $Arguments,
        [int] $WaitSeconds,
        [string] $What
    )
    Write-Host "[UAK] 启动：$What"
    $proc = Start-Process -FilePath $Exe -ArgumentList $Arguments -PassThru -WindowStyle Minimized
    if ($WaitSeconds -gt 0) {
        $exited = $proc.WaitForExit($WaitSeconds * 1000)
        if (-not $exited) {
            Write-Host "[UAK] $What 达到 ${WaitSeconds}s，结束进程"
            $proc.Kill()
            $proc.WaitForExit(15000) | Out-Null
        }
    }
    Write-Host "[UAK] $What 退出码：$($proc.ExitCode)"
    return $proc.ExitCode
}

# ── Stage 1：编辑器内 Automation ────────────────────────────────────────
if ($Stage -in @('All', 'Editor')) {
    $stageReport = Join-Path $reportDir 'Editor'
    $stageLog    = Join-Path $logDir "Editor-$stamp.log"
    New-Item -ItemType Directory -Force -Path $stageReport | Out-Null

    $args = @(
        "`"$uproject`"",
        "-ExecCmds=`"Automation RunTests $Filter; Quit`"",
        "-TestExit=`"Automation Test Queue Empty`"",
        "-ReportExportPath=`"$stageReport`"",
        '-unattended', '-nopause', '-nosplash',
        "-abslog=`"$stageLog`""
    )
    if ($Headless) {
        $args += '-NullRHI'
    }
    else {
        # 真实 RHI 才有像素；-UAKCaptureMedia 告诉测试模块开启逐帧截图
        $args += @('-Windowed', '-ResX=1280', '-ResY=720', '-UAKCaptureMedia=1')
    }

    Invoke-UakProcess -Exe $editor -Arguments $args -WaitSeconds $TimeoutSeconds -What 'Stage Editor' | Out-Null
    $reportDirs += $stageReport
    $logFiles   += $stageLog
}

# ── Stage 2：独立进程（-game）里的真实游戏世界 ──────────────────────────
if ($Stage -in @('All', 'Game')) {
    $stageLog = Join-Path $logDir "Game-$stamp.log"

    $execCmds = @()
    if ($Map) { $execCmds += "open $Map" }
    # 周期快照：进程被脚本结束时，最后一份文件就是结束时刻的状态
    $execCmds += 'UAK.Auto.Snapshot Client0 2'
    $execCmds += 'UAK.Auto.Record Game.Playback 300 30'
    if ($AbilityClass) { $execCmds += "UAK.Auto.ActivateAbility $AbilityClass 3" }
    $exec = ($execCmds -join '; ')

    $args = @(
        "`"$uproject`"", '-game',
        "-ExecCmds=`"$exec`"",
        '-unattended', '-nopause', '-nosplash',
        "-abslog=`"$stageLog`""
    )
    if ($Headless) { $args += '-NullRHI' }
    else { $args += @('-Windowed', '-ResX=1280', '-ResY=720') }

    # 不给 Quit：录满 $GameSeconds 秒后由脚本收尾
    Invoke-UakProcess -Exe $editor -Arguments $args -WaitSeconds $GameSeconds -What 'Stage Game' | Out-Null
    $logFiles += $stageLog
}

# ── Stage 3：DS + N 个客户端 ────────────────────────────────────────────
if ($Stage -in @('All', 'Net')) {
    $serverLog = Join-Path $logDir "Server-$stamp.log"
    # 用编辑器二进制起 DS（不用 Server target：本地不需要 cook）
    $serverArgs = @(
        "`"$uproject`"", '-server',
        "-Port=$Port",
        '-ExecCmds="UAK.Auto.Snapshot Server 2"',
        '-unattended', '-nopause', '-nosplash', '-NullRHI',
        "-abslog=`"$serverLog`""
    )
    Write-Host "[UAK] 启动：DS（端口 $Port）"
    $server = Start-Process -FilePath $editor -ArgumentList $serverArgs -PassThru -WindowStyle Minimized
    Start-Sleep -Seconds $ServerBootWait
    $logFiles += $serverLog

    $clients = @()
    for ($i = 1; $i -le $Clients; $i++) {
        $clientLog = Join-Path $logDir "Client$i-$stamp.log"
        $execCmds = @(
            "open 127.0.0.1:$Port",
            "UAK.Auto.Snapshot Client$i 2",
            "UAK.Auto.Record Net.Client$i 300 30"
        )
        if ($AbilityClass) { $execCmds += "UAK.Auto.ActivateAbility $AbilityClass 3" }
        $exec = ($execCmds -join '; ')

        $args = @(
            "`"$uproject`"", '-game',
            "-ExecCmds=`"$exec`"",
            '-unattended', '-nopause', '-nosplash',
            "-abslog=`"$clientLog`""
        )
        if ($Headless) { $args += '-NullRHI' }
        else { $args += @('-Windowed', '-ResX=640', '-ResY=360') }

        Write-Host "[UAK] 启动：客户端 $i"
        $clients += Start-Process -FilePath $editor -ArgumentList $args -PassThru -WindowStyle Minimized
        $logFiles += $clientLog
        Start-Sleep -Seconds 3   # 错开登录，避免同时抢登录位
    }

    Write-Host "[UAK] 让多端跑 $NetSeconds 秒…"
    Start-Sleep -Seconds $NetSeconds

    foreach ($c in $clients) {
        if (-not $c.HasExited) { $c.Kill(); $c.WaitForExit(15000) | Out-Null }
    }
    if ($server -and -not $server.HasExited) {
        # 等一个快照周期，确保最后一份 Server.json 是最新状态
        Start-Sleep -Seconds 3
        $server.Kill()
        $server.WaitForExit(15000) | Out-Null
    }
    Write-Host "[UAK] Stage Net 结束"
}

# ── 汇总报告 ────────────────────────────────────────────────────────────
$generator = Join-Path $PSScriptRoot 'report\generate_report.py'
$reportExit = 0
if (Get-Command $Python -ErrorAction SilentlyContinue) {
    $pyArgs = @(
        $generator,
        '--project', $Project,
        '--out', 'docs/reports',
        '--stamp', (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
    )
    foreach ($d in $reportDirs) { $pyArgs += @('--report-dir', $d) }
    foreach ($l in $logFiles)   { $pyArgs += @('--log-file', $l) }

    & $Python @pyArgs
    $reportExit = $LASTEXITCODE
}
else {
    Write-Warning "[UAK] 未找到 python，跳过报告生成（原始数据仍在 $baseDir）"
}

Write-Host "[UAK] 报告输出：$(Join-Path $Project 'docs\reports')"

# ── 可选：把结论提交回仓库（CI 只负责渲染 Pages）────────────────────────
if ($Commit) {
    Push-Location $Project
    try {
        & git add docs/reports docs/showcase.md
        & git diff --staged --quiet
        if ($LASTEXITCODE -ne 0) {
            & git commit -m "chore: update automation report ($stamp)"
            Write-Host "[UAK] 已提交测试结论"
        }
        else {
            Write-Host "[UAK] 结论没有变化，跳过提交"
        }
        if ($Push) {
            & git push
        }
    }
    finally { Pop-Location }
}

exit $reportExit
