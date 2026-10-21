<#
.SYNOPSIS
    一键清除本工程里所有从 Lyra 拷进来的内容，回到"还没导入 Lyra"的干净状态。

.DESCRIPTION
    为什么要有这一步：
        拷进来的 Lyra 一旦被人手改过，编译/测试就会"因为改过而通过"，把真正的 bug 藏起来。
        所以每次重新导入前先清一遍，保证工程永远等于：
            Lyra 原版 + Tools/lyra_patches.json 里声明过的补丁（由 patch_lyra.py 自动打）
        没有任何"手改"。

    清什么：
        - 源码：Source/LyraGame、Source/LyraEditor、Source/*.Target.cs
        - 外围：根目录 *.uproject、Config/、Platforms/
        - 资产：Plugins/ 下除 -KeepPlugin 之外的所有插件（GameFeatures 等，约 1.4 GB）
        - 补丁痕迹：*.orig、*.uak.bak、Tools/.patched_files.txt、Tools/.lyra_state.json
        - 导入清单：Tools/.lyra_paths.txt、Tools/lyra_manifest.txt

    不清什么（这些是我们自己的）：
        Plugins/UEAbilityKit、Plugins/UnLuaNova、Source/LyraAdapt、
        Content/UAK、Content/Script、Tools/patches/**

.EXAMPLE
    ./clean_lyra.ps1                 # 清源码 + 配置 + 资产
    ./clean_lyra.ps1 -KeepAssets     # 只清源码/配置，1.4 GB 资产留着（迭代时快）
    ./clean_lyra.ps1 -Deep           # 连 Binaries/Intermediate/DerivedDataCache/Saved 一起清
    ./clean_lyra.ps1 -DryRun         # 只列出来，不真删
#>
[CmdletBinding()]
param(
    [switch] $KeepAssets,                                   # 保留从 Lyra 拷来的资产
    [switch] $Deep,                                         # 连引擎生成物一起清
    [switch] $DryRun,                                       # 预演
    [string[]] $KeepPlugin = @('UEAbilityKit', 'UnLuaNova') # 我们自己的插件，永不删
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path.TrimEnd('\')
$Guard = $RepoRoot + '\'

$script:Removed = 0

function Remove-UakPath {
    param([Parameter(Mandatory)][string] $Rel)

    $full = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $Rel))
    if (-not $full.StartsWith($Guard, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "[clean] 拒绝删除仓库之外的路径：$full"
    }
    if (-not (Test-Path -LiteralPath $full)) { return }

    $show = $full.Substring($Guard.Length)
    if ($DryRun) {
        Write-Host "[clean] 将删除：$show"
        $script:Removed++
        return
    }
    Remove-Item -LiteralPath $full -Recurse -Force
    Write-Host "[clean] 删除：$show"
    $script:Removed++
}

Write-Host "[clean] 工程根目录：$RepoRoot"

# ── 1. 上次导入的清单（有的话就照单全删，最准）────────────────────────────
$pathsFile = Join-Path $PSScriptRoot '.lyra_paths.txt'
if (Test-Path $pathsFile) {
    foreach ($line in (Get-Content $pathsFile)) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        Remove-UakPath $line.Trim()
    }
}

# ── 2. 固定清单（清单文件丢了也能清干净）──────────────────────────────────
foreach ($rel in @(
        'Source\LyraGame',
        'Source\LyraEditor',
        'Config',
        'Platforms',
        'Content\ShooterCore',
        'Content\ShooterMaps'
    )) { Remove-UakPath $rel }

foreach ($f in (Get-ChildItem -Path (Join-Path $RepoRoot 'Source') -Filter '*.Target.cs' -File -ErrorAction SilentlyContinue)) {
    Remove-UakPath "Source\$($f.Name)"
}
foreach ($f in (Get-ChildItem -Path $RepoRoot -Filter '*.uproject' -File -ErrorAction SilentlyContinue)) {
    Remove-UakPath $f.Name          # 根目录的 .uproject 整份都是 Lyra 的
}

# ── 3. 资产：Plugins/ 下除了我们自己的插件，其余都是 Lyra 拷来的 ──────────
if (-not $KeepAssets) {
    $pluginRoot = Join-Path $RepoRoot 'Plugins'
    foreach ($p in (Get-ChildItem -Path $pluginRoot -Directory -ErrorAction SilentlyContinue)) {
        if ($KeepPlugin -contains $p.Name) {
            Write-Host "[clean] 保留（我们的）：Plugins\$($p.Name)"
            continue
        }
        Remove-UakPath "Plugins\$($p.Name)"
    }
}
else {
    Write-Host '[clean] -KeepAssets：保留资产插件（注意：资产不会重新校验，只是省时间）'
}

# ── 4. 补丁痕迹 ───────────────────────────────────────────────────────────
Get-ChildItem -Path $RepoRoot -Recurse -File -Include '*.orig', '*.uak.bak' -ErrorAction SilentlyContinue |
    ForEach-Object {
        $rel = $_.FullName.Substring($Guard.Length)
        Remove-UakPath $rel
    }

foreach ($rel in @(
        'Tools\.patched_files.txt',
        'Tools\.lyra_state.json',
        'Tools\.lyra_paths.txt',
        'Tools\lyra_manifest.txt'
    )) { Remove-UakPath $rel }

# ── 5. 引擎生成物（可选）──────────────────────────────────────────────────
if ($Deep) {
    foreach ($rel in @('Binaries', 'Intermediate', 'DerivedDataCache', 'Saved')) { Remove-UakPath $rel }
    Write-Host '[clean] -Deep：已清引擎生成物（下次编译会是全量，慢但最干净）'
}

Write-Host ""
if ($DryRun) {
    Write-Host "[clean] 预演完成：共 $script:Removed 项将被删除（未改动任何文件）"
}
else {
    Write-Host "[clean] 完成：删除 $script:Removed 项，工程已回到未导入 Lyra 的状态"
    Write-Host "[clean] 下一步：./import_lyra.ps1 -LyraPath <你的 Lyra 目录>"
}
