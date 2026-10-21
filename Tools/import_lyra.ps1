<#
.SYNOPSIS
    把下载好的 Lyra 拷进本工程并打补丁——本工程由此变成"改造过后的 Lyra"。

.DESCRIPTION
    流程：
      1. 校验 Lyra 路径与引擎版本
      2. 拷源码：Source/LyraGame（子集，完整可玩那套）+ Source/LyraEditor
      3. 拷外围文件：.uproject / Target.cs / Config（以 Lyra 为基线，原文件留 .uak.bak）
      4. 拷资产：Plugins/GameFeatures/* 等（约 1.4 GB，不进 git，已加 .gitignore）
      5. 打补丁：ULyraGameplayAbility 改为继承 UAKGameplayAbility（Tools/patch_lyra.py）

    改名策略：拷贝进来的 Lyra 代码**保持 Lyra 命名**（保留 Epic 版权头与来源），
    我们的东西用 AK 命名。NOTICE 里记录来源。

.EXAMPLE
    ./import_lyra.ps1 -LyraPath D:\learning\LyraStarterGame
    ./import_lyra.ps1 -LyraPath D:\learning\LyraStarterGame -SkipAssets   # 只拷源码，不拷 1.4GB 资产
    ./import_lyra.ps1 -LyraPath D:\learning\LyraStarterGame -Force        # 覆盖已存在的文件
#>
[CmdletBinding()]
param(
    [string] $LyraPath   = '',
    [string] $EngineRoot = $env:UE_ENGINE_ROOT,
    [switch] $SkipAssets,
    [switch] $SkipPatch,
    [switch] $Force,
    [string] $Python     = 'python'
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

# ── 1. 校验 ─────────────────────────────────────────────────────────────
if ([string]::IsNullOrWhiteSpace($LyraPath)) { throw "请用 -LyraPath 指定下载好的 Lyra 工程目录" }
if (-not (Test-Path $LyraPath)) { throw "Lyra 目录不存在：$LyraPath" }
$LyraPath = (Resolve-Path $LyraPath).Path

$uproject = $null
foreach ($name in @('LyraStarterGame.uproject', 'Lyra.uproject')) {
    $candidate = Join-Path $LyraPath $name
    if (Test-Path $candidate) { $uproject = $candidate; break }
}
if (-not $uproject) {
    $found = Get-ChildItem -Path $LyraPath -Filter '*.uproject' -File | Select-Object -First 1
    if (-not $found) { throw "$LyraPath 下找不到 .uproject" }
    $uproject = $found.FullName
}

$engineVersion = (Get-Content $uproject -Raw | ConvertFrom-Json).EngineAssociation
Write-Host "[import] Lyra：$uproject（引擎 $engineVersion）"
if ($engineVersion -and $engineVersion -notmatch '^5\.(4|6|8)') {
    Write-Warning "[import] UAK 验证过 5.4 / 5.6 / 5.8，当前版本 $engineVersion 建议先确认"
}

$manifest = @()

# ── 2. 拷源码 ───────────────────────────────────────────────────────────
# "完整可玩"需要的模块：能力 / 角色 / 玩家 / 输入 / 相机 / 武器 / 装备 / 库存 / 队伍 / UI 等。
# 刻意排除：Settings、Hotfix、Performance、Replays、Development、Tests（与技能主线无关）。
$LyraDirs = @(
    'AbilitySystem', 'Animation', 'Audio', 'Camera', 'Character', 'Cosmetics',
    'Equipment', 'Feedback', 'GameFeatures', 'GameModes', 'Input', 'Interaction',
    'Inventory', 'Messages', 'Physics', 'Player', 'System', 'Teams', 'Weapons', 'UI'
)

$destRoot = Join-Path $RepoRoot 'Source\LyraGame'
New-Item -ItemType Directory -Force -Path $destRoot | Out-Null

# 模块根上的文件（Build.cs / Module.cpp / GameplayTags / LogChannels 等）
foreach ($file in Get-ChildItem -Path (Join-Path $LyraPath 'Source\LyraGame') -File -ErrorAction SilentlyContinue) {
    $target = Join-Path $destRoot $file.Name
    if ((Test-Path $target) -and -not $Force) { continue }
    Copy-Item $file.FullName $target -Force
    $manifest += "Source/LyraGame/$($file.Name)"
}

foreach ($dir in $LyraDirs) {
    $src = Join-Path $LyraPath "Source\LyraGame\$dir"
    if (-not (Test-Path $src)) { Write-Warning "[import] Lyra 里没有 $dir，跳过"; continue }

    $dest = Join-Path $destRoot $dir
    if ((Test-Path $dest) -and -not $Force) {
        Write-Host "[import] 已存在，跳过：$dir（-Force 可覆盖）"
        continue
    }
    Copy-Item $src $dest -Recurse -Force
    $manifest += "Source/LyraGame/$dir"
    Write-Host "[import] 源码：$dir"
}

# LyraEditor（编辑器工具，小）
$editorSrc = Join-Path $LyraPath 'Source\LyraEditor'
if (Test-Path $editorSrc) {
    $editorDest = Join-Path $RepoRoot 'Source\LyraEditor'
    if (-not (Test-Path $editorDest) -or $Force) {
        Copy-Item $editorSrc $editorDest -Recurse -Force
        $manifest += 'Source/LyraEditor'
        Write-Host '[import] 源码：LyraEditor'
    }
}

# ── 3. 外围文件：copy（Lyra 是基线，我们的东西靠后面的 patch 加回去）──
Write-Host "[import] 拷外围文件（以 Lyra 为基线）…"

# 3a. .uproject：整个就是 Lyra 的（连名字一起），本工程 = 改造过后的 Lyra
$ourUproject = Join-Path $RepoRoot (Split-Path $uproject -Leaf)
if ((Test-Path $ourUproject) -and -not $Force) {
    Copy-Item $ourUproject "$ourUproject.uak.bak" -Force
}
Copy-Item $uproject $ourUproject -Force
Write-Host "[import] .uproject：$(Split-Path $uproject -Leaf) ← Lyra"

# 说明：UAKDemo 现在是**插件的一个模块**（Plugins/UEAbilityKit/Source/UAKDemo），
# 不再是独立工程——所以这里不需要删任何东西。宿主是 Lyra，示例随插件走。

# 3b. Config
$cfgDest = Join-Path $RepoRoot 'Config'
New-Item -ItemType Directory -Force -Path $cfgDest | Out-Null
foreach ($ini in Get-ChildItem -Path (Join-Path $LyraPath 'Config') -Filter '*.ini' -File) {
    $target = Join-Path $cfgDest $ini.Name
    if ((Test-Path $target) -and -not $Force) { Copy-Item $target "$target.uak.bak" -Force }
    Copy-Item $ini.FullName $target -Force
}
Write-Host '[import] Config ← Lyra'

# 3c. Target.cs
foreach ($t in Get-ChildItem -Path (Join-Path $LyraPath 'Source') -Filter '*.Target.cs' -File) {
    $dest = Join-Path $RepoRoot "Source\$($t.Name)"
    if ((Test-Path $dest) -and -not $Force) { Copy-Item $dest "$dest.uak.bak" -Force }
    Copy-Item $t.FullName $dest -Force
}
Write-Host '[import] Target.cs ← Lyra'

# ── 4. 拷资产（大，不进 git）─────────────────────────────────────────────
if (-not $SkipAssets) {
    Write-Host "[import] 拷资产（约 1.4 GB，不进 git）…"

    $pluginDest = Join-Path $RepoRoot 'Plugins'
    New-Item -ItemType Directory -Force -Path $pluginDest | Out-Null

    # 4a. GameFeature 插件：武器、关卡都在这（ShooterCore / ShooterMaps / ...）
    $gfSrc = Join-Path $LyraPath 'Plugins\GameFeatures'
    if (Test-Path $gfSrc) {
        $gfDest = Join-Path $pluginDest 'GameFeatures'
        New-Item -ItemType Directory -Force -Path $gfDest | Out-Null
        foreach ($gf in Get-ChildItem -Path $gfSrc -Directory) {
            $target = Join-Path $gfDest $gf.Name
            if ((Test-Path $target) -and -not $Force) { continue }
            Copy-Item $gf.FullName $target -Recurse -Force
            Write-Host "[import] 资产插件：GameFeatures/$($gf.Name)"
        }
    }

    # 4b. Lyra 自带的插件：5.8 引擎里已有的不拷，避免重名冲突
    if ($EngineRoot -and (Test-Path (Join-Path $EngineRoot 'Engine\Plugins'))) {
        $enginePluginDir = Join-Path $EngineRoot 'Engine\Plugins'
        foreach ($p in Get-ChildItem -Path (Join-Path $LyraPath 'Plugins') -Directory) {
            if ($p.Name -eq 'GameFeatures') { continue }
            if (Test-Path (Join-Path $enginePluginDir $p.Name)) {
                Write-Host "[import] 引擎自带，跳过：$($p.Name)"
                continue
            }
            $target = Join-Path $pluginDest $p.Name
            if ((Test-Path $target) -and -not $Force) { continue }
            Copy-Item $p.FullName $target -Recurse -Force
            Write-Host "[import] Lyra 插件：$($p.Name)"
        }
    }
    else {
        Write-Warning "[import] 未指定 -EngineRoot，跳过 Lyra 自带插件的筛选拷贝"
    }

    # 4c. 平台配置（小）
    $platformSrc = Join-Path $LyraPath 'Platforms'
    if (Test-Path $platformSrc) {
        Copy-Item $platformSrc (Join-Path $RepoRoot 'Platforms') -Recurse -Force
        Write-Host '[import] 平台配置：Platforms/'
    }
}

# ── 5. 打补丁：Lyra 继承 UAK ────────────────────────────────────────────
if (-not $SkipPatch) {
    if (Get-Command $Python -ErrorAction SilentlyContinue) {
        & $Python (Join-Path $PSScriptRoot 'patch_lyra.py')
        if ($LASTEXITCODE -ne 0) { throw "打补丁失败" }
    }
    else {
        Write-Warning "[import] 没有 python，请手动让 ULyraGameplayAbility 继承 UAKGameplayAbility"
    }
}

# ── 6. 记录来源 ─────────────────────────────────────────────────────────
$manifestPath = Join-Path $PSScriptRoot 'lyra_manifest.txt'
$manifest | Out-File -FilePath $manifestPath -Encoding utf8
Write-Host "[import] 来源清单：$manifestPath"

Write-Host ""
Write-Host "[import] 完成。下一步："
Write-Host "  1) 编译：Scripts\build.ps1（或 UnrealBuildTool 编 UAKDemoEditor）"
Write-Host "  2) 打开工程，确认 ULyraGameplayAbility 已经能用 UAK 时间轴"
Write-Host "  3) 把 Lyra 的 BP 技能按 UAK 资产重写（见 docs/examples/lyra-integration.md）"
