<#
.SYNOPSIS
    UAK 命令行编译脚本：直接调用 UnrealBuildTool，不经过 Build.bat。

.DESCRIPTION
    为什么不走 Build.bat：
      Build.bat 用 %TEMP%\<路径>.lock 做互斥，一旦某个实例异常退出留下句柄，
      后续所有 Build.bat 都会卡在 "already running, waiting for existing script
      to terminate..."，而锁文件删不掉（被占用）。
      直接调 UnrealBuildTool.dll 完全不碰这个锁，并且自带 -WaitMutex 做并发保护。

    与 Build.bat 等价：产出同样的 Binaries/Intermediate，VS 里可直接调试。

.EXAMPLE
    ./Scripts/build.ps1                          # UAKDemoEditor / Win64 / Development
    ./Scripts/build.ps1 -Target UAKDemo -Configuration Shipping
    ./Scripts/build.ps1 -Clean
#>
[CmdletBinding()]
param(
    [string] $Target        = 'UAKDemoEditor',
    [string] $Platform      = 'Win64',
    [string] $Configuration = 'Development',
    [string] $Project       = '',
    [string] $EngineRoot    = $env:UE_ENGINE_ROOT,
    [switch] $Clean,
    [int]    $TimeoutSeconds = 3600
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Project)) {
    $Project = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
}
$Project  = (Resolve-Path $Project).Path
$uproject = Join-Path $Project 'UAKDemo.uproject'
if (-not (Test-Path $uproject)) { throw "找不到工程文件：$uproject" }

# 引擎路径：显式指定 > 按 .uproject 的 EngineAssociation GUID 反查注册表
if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $assoc = (Get-Content $uproject -Raw | ConvertFrom-Json).EngineAssociation
    if ($assoc -match '^\{[0-9A-Fa-f-]+\}$') {
        $EngineRoot = (Get-ItemProperty -Path 'HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds' `
            -Name $assoc -ErrorAction SilentlyContinue).$assoc
    }
}
if ([string]::IsNullOrWhiteSpace($EngineRoot) -or -not (Test-Path $EngineRoot)) {
    throw "找不到 UE 引擎：用 -EngineRoot 指定，或设置环境变量 UE_ENGINE_ROOT"
}

$ubt    = Join-Path $EngineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'
$srcDir = Join-Path $EngineRoot 'Engine\Source'
if (-not (Test-Path $ubt))    { throw "缺少 UBT：$ubt" }
if (-not (Test-Path $srcDir)) { throw "缺少引擎源码目录：$srcDir" }

# 隔离 TEMP：万一 UBT 内部派生 Build.bat（AutoSDK 校验等），也不会撞上别人的锁
$privateTemp = Join-Path $Project 'Saved\PrivateTemp'
New-Item -ItemType Directory -Force -Path $privateTemp | Out-Null
$env:TEMP = $privateTemp
$env:TMP  = $privateTemp

Write-Host "[UAK] 工程  ：$Project"
Write-Host "[UAK] 引擎  ：$EngineRoot"
Write-Host "[UAK] 目标  ：$Target $Platform $Configuration"
Write-Host "[UAK] TEMP  ：$privateTemp（已隔离，避免 Build.bat 锁冲突）"

$ubtArgs = @($Target, $Platform, $Configuration, "-Project=$uproject", '-WaitMutex', '-Progress')
if ($Clean) { $ubtArgs += '-Clean' }

Push-Location $srcDir
try {
    $proc = Start-Process -FilePath 'dotnet' -ArgumentList (@("`"$ubt`"") + $ubtArgs) `
        -NoNewWindow -PassThru -Wait:$false

    $done = $proc.WaitForExit($TimeoutSeconds * 1000)
    if (-not $done) {
        Write-Warning "[UAK] 超时 ${TimeoutSeconds}s，强制结束"
        $proc.Kill()
        throw "编译超时"
    }

    Write-Host "[UAK] 退出码：$($proc.ExitCode)"
    if ($proc.ExitCode -ne 0) { throw "编译失败（exit=$($proc.ExitCode)）" }
}
finally {
    Pop-Location
}

Write-Host "[UAK] 编译完成"
