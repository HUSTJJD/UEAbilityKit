<#
.SYNOPSIS
    把下载好的 Lyra 拷进本工程并打补丁——本工程由此变成"改造过后的 Lyra"。

.DESCRIPTION
    流程（每一步都可重跑，结果都一样）：
      0. 先清：Tools/clean_lyra.ps1 —— 保证工程里没有任何上一次导入的残留与手改
      1. 校验 Lyra 路径与引擎版本
      2. 拷源码：Source/LyraGame（子集，完整可玩那套）+ Source/LyraEditor
      3. 拷外围：.uproject / Target.cs / Config（以 Lyra 为基线，我们的东西靠补丁加回来）
      4. 拷资产：Plugins/GameFeatures/* 等（约 1.4 GB，不进 git）
      5. 打补丁：ULyraGameplayAbility 继承 UAKGameplayAbility 等（Tools/patch_lyra.py）
      6. 记清单 + 校验：Tools/.lyra_paths.txt（清的时候照单全删）、
         Tools/.lyra_state.json（快照，之后 verify_lyra.py 可离线比对）

    铁律：**不改 Lyra 的内容**。
    必须改的地方全部写在 Tools/lyra_patches.json 里，由第 5 步自动打；
    手改过的 Lyra 文件会让测试"因为改过而通过"，把 bug 藏起来，
    所以每次导入都从 clean 开始，导入完立刻 verify。

    改名策略：拷贝进来的 Lyra 代码**保持 Lyra 命名**（保留 Epic 版权头与来源），
    我们的东西用 AK 命名。NOTICE 里记录来源。

.EXAMPLE
    ./import_lyra.ps1 -LyraPath D:\learning\LyraStarterGame
    ./import_lyra.ps1 -LyraPath D:\learning\LyraStarterGame -SkipAssets   # 只拷源码，不拷 1.4GB 资产
    ./import_lyra.ps1 -LyraPath D:\learning\LyraStarterGame -KeepAssets   # 清源码/配置，资产沿用上次
    ./import_lyra.ps1 -LyraPath D:\learning\LyraStarterGame -NoClean      # 不预清（不推荐，只在调试时用）
#>
[CmdletBinding()]
param(
    [string] $LyraPath   = '',
    [string] $EngineRoot = $env:UE_ENGINE_ROOT,
    [switch] $SkipAssets,
    [switch] $SkipPatch,
    [switch] $KeepAssets,   # 与 clean_lyra 同义：1.4 GB 资产不重新拷
    [switch] $NoClean,      # 跳过预清理（不推荐）
    [switch] $Force,        # 兼容旧用法：现在默认就是"清完重拷"，这个开关不再影响行为
    [string] $Python     = 'python'
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

$script:Imported = New-Object System.Collections.Generic.List[string]
$script:Manifest = New-Object System.Collections.Generic.List[string]   # 给人看的来源清单

# ── 拷贝 + 记账（记下来，clean_lyra 才知道该删什么）────────────────────────
function Copy-Lyra {
    param(
        [Parameter(Mandatory)][string] $Src,
        [Parameter(Mandatory)][string] $Rel,   # 相对工程根的目标路径
        [switch] $Dir
    )
    $dest = Join-Path $RepoRoot $Rel
    New-Item -ItemType Directory -Force -Path (Split-Path $dest -Parent) | Out-Null
    Copy-Item -LiteralPath $Src -Destination $dest -Recurse:$Dir -Force
    $script:Imported.Add($Rel)
}

# ── 0. 校验 ─────────────────────────────────────────────────────────────
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

# ── 0b. 先清干净：没有这一步，上一次的手改会留下来骗过测试 ────────────────
if (-not $NoClean) {
    Write-Host "[import] 预清理（保证这次导入是唯一的变量来源）…"
    & (Join-Path $PSScriptRoot 'clean_lyra.ps1') -KeepAssets:$KeepAssets
    if (-not $?) { throw "预清理失败" }   # 脚本内部没有 native 调用，$LASTEXITCODE 不可靠，看 $?
}
else {
    Write-Warning "[import] -NoClean：跳过预清理，工程里可能残留上次的手改（只用于调试）"
}

# ── 1. 拷源码 ───────────────────────────────────────────────────────────
# "完整可玩"需要的模块：能力 / 角色 / 玩家 / 输入 / 相机 / 武器 / 装备 / 库存 / 队伍 / UI 等。
# 刻意排除：Settings、Hotfix、Performance、Replays、Development、Tests（与技能主线无关）。
$LyraDirs = @(
    'AbilitySystem', 'Animation', 'Audio', 'Camera', 'Character', 'Cosmetics',
    'Equipment', 'Feedback', 'GameFeatures', 'GameModes', 'Input', 'Interaction',
    'Inventory', 'Messages', 'Physics', 'Player', 'System', 'Teams', 'Weapons', 'UI'
)

foreach ($file in Get-ChildItem -Path (Join-Path $LyraPath 'Source\LyraGame') -File -ErrorAction SilentlyContinue) {
    Copy-Lyra -Src $file.FullName -Rel "Source\LyraGame\$($file.Name)"
}

foreach ($dir in $LyraDirs) {
    $src = Join-Path $LyraPath "Source\LyraGame\$dir"
    if (-not (Test-Path $src)) { Write-Warning "[import] Lyra 里没有 $dir，跳过"; continue }
    Copy-Lyra -Src $src -Rel "Source\LyraGame\$dir" -Dir
    $script:Manifest.Add("Source/LyraGame/$dir")
    Write-Host "[import] 源码：$dir"
}

# LyraEditor（编辑器工具，小）
$editorSrc = Join-Path $LyraPath 'Source\LyraEditor'
if (Test-Path $editorSrc) {
    Copy-Lyra -Src $editorSrc -Rel 'Source\LyraEditor' -Dir
    $script:Manifest.Add('Source/LyraEditor')
    Write-Host '[import] 源码：LyraEditor'
}

# ── 2. 外围文件：copy（Lyra 是基线，我们的东西靠后面的 patch 加回去）──
Write-Host "[import] 拷外围文件（以 Lyra 为基线）…"

# 2a. .uproject：整个就是 Lyra 的（连名字一起），本工程 = 改造过后的 Lyra
$uprojectName = Split-Path $uproject -Leaf
Copy-Lyra -Src $uproject -Rel $uprojectName
Write-Host "[import] .uproject：$uprojectName ← Lyra"

# 说明：UAKDemo 现在是**插件的一个模块**（Plugins/UEAbilityKit/Source/UAKDemo），
# 不再是独立工程——所以这里不需要删任何东西。宿主是 Lyra，示例随插件走。

# 2b. Config
foreach ($ini in Get-ChildItem -Path (Join-Path $LyraPath 'Config') -Filter '*.ini' -File -ErrorAction SilentlyContinue) {
    Copy-Lyra -Src $ini.FullName -Rel "Config\$($ini.Name)"
}
Write-Host '[import] Config ← Lyra'

# 2c. Target.cs
foreach ($t in Get-ChildItem -Path (Join-Path $LyraPath 'Source') -Filter '*.Target.cs' -File -ErrorAction SilentlyContinue) {
    Copy-Lyra -Src $t.FullName -Rel "Source\$($t.Name)"
}
Write-Host '[import] Target.cs ← Lyra'

# ── 3. 拷资产（大，不进 git）─────────────────────────────────────────────
if (-not $SkipAssets) {
    Write-Host "[import] 拷资产（约 1.4 GB，不进 git）…"

    # 3a. GameFeature 插件：武器、关卡都在这（ShooterCore / ShooterMaps / ...）
    $gfSrc = Join-Path $LyraPath 'Plugins\GameFeatures'
    if (Test-Path $gfSrc) {
        foreach ($gf in Get-ChildItem -Path $gfSrc -Directory) {
            Copy-Lyra -Src $gf.FullName -Rel "Plugins\GameFeatures\$($gf.Name)" -Dir
            Write-Host "[import] 资产插件：GameFeatures/$($gf.Name)"
        }
    }

    # 3b. Lyra 自带的插件：5.8 引擎里已有的不拷，避免重名冲突
    if ($EngineRoot -and (Test-Path (Join-Path $EngineRoot 'Engine\Plugins'))) {
        $enginePluginDir = Join-Path $EngineRoot 'Engine\Plugins'
        foreach ($p in Get-ChildItem -Path (Join-Path $LyraPath 'Plugins') -Directory) {
            if ($p.Name -eq 'GameFeatures') { continue }
            if (Test-Path (Join-Path $enginePluginDir $p.Name)) {
                Write-Host "[import] 引擎自带，跳过：$($p.Name)"
                continue
            }
            Copy-Lyra -Src $p.FullName -Rel "Plugins\$($p.Name)" -Dir
            Write-Host "[import] Lyra 插件：$($p.Name)"
        }
    }
    else {
        # 没给引擎目录就别自作聪明地跳过：Lyra 的插件缺一个就编译不过，宁可全拷
        Write-Warning "[import] 未指定 -EngineRoot：Lyra 自带插件不做筛选，全部拷进来（可能与引擎自带插件重名）"
        foreach ($p in Get-ChildItem -Path (Join-Path $LyraPath 'Plugins') -Directory) {
            if ($p.Name -eq 'GameFeatures') { continue }
            Copy-Lyra -Src $p.FullName -Rel "Plugins\$($p.Name)" -Dir
            Write-Host "[import] Lyra 插件：$($p.Name)"
        }
    }

    # 3c. 平台配置（小）
    $platformSrc = Join-Path $LyraPath 'Platforms'
    if (Test-Path $platformSrc) {
        Copy-Lyra -Src $platformSrc -Rel 'Platforms' -Dir
        Write-Host '[import] 平台配置：Platforms/'
    }
}

# ── 4. 打补丁：Lyra 继承 UAK（唯一允许改动 Lyra 的地方）──────────────────
if (-not $SkipPatch) {
    if (Get-Command $Python -ErrorAction SilentlyContinue) {
        & $Python (Join-Path $PSScriptRoot 'patch_lyra.py')
        if ($LASTEXITCODE -ne 0) { throw "打补丁失败" }
    }
    else {
        Write-Warning "[import] 没有 python，请手动让 ULyraGameplayAbility 继承 UAKGameplayAbility"
    }
}

# ── 5. 记账：清的时候照单全删；记快照：之后可离线校验 ────────────────────
$pathsFile = Join-Path $PSScriptRoot '.lyra_paths.txt'
$script:Imported | Sort-Object -Unique | Out-File -FilePath $pathsFile -Encoding utf8
Write-Host "[import] 导入清单（clean_lyra 按它删）：$pathsFile"

$manifestPath = Join-Path $PSScriptRoot 'lyra_manifest.txt'
$script:Manifest | Sort-Object -Unique | Out-File -FilePath $manifestPath -Encoding utf8
Write-Host "[import] 来源清单：$manifestPath"

# ── 6. 校验：确认工程 = Lyra 原版 + 声明过的补丁 ─────────────────────────
if (Get-Command $Python -ErrorAction SilentlyContinue) {
    & $Python (Join-Path $PSScriptRoot 'verify_lyra.py') -l $LyraPath --update-state
    if ($LASTEXITCODE -ne 0) { Write-Warning "[import] 校验没通过：上面列出的文件与 Lyra 原版不一致" }
}

Write-Host ""
Write-Host "[import] 完成。下一步："
Write-Host "  1) 编译：Scripts\build.ps1（或 UnrealBuildTool 编 LyraGameEditor）"
Write-Host "  2) 打开工程，确认 ULyraGameplayAbility 已经能用 UAK 时间轴"
Write-Host "  3) 把 Lyra 的 BP 技能按 UAK 资产重写（见 docs/examples/lyra-integration.md）"
Write-Host ""
Write-Host "[import] 随时回到干净状态：Tools\clean_lyra.ps1（然后重跑本脚本即可复现）"
