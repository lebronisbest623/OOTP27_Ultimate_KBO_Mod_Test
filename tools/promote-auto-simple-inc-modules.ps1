param(
    [int] $Limit = 5,
    [switch] $IncludeStateful,
    [switch] $WhatIf
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$NativeRoot = Join-Path $RepoRoot "native"
$PromoteScript = Join-Path $PSScriptRoot "promote-static-inc-module.ps1"

function Get-RelativeNativePath {
    param([string] $Path)

    $FullPath = (Resolve-Path -LiteralPath $Path).Path
    $NativeFull = (Resolve-Path -LiteralPath $NativeRoot).Path
    return $FullPath.Substring($NativeFull.Length + 1).Replace("\", "/")
}

function Test-SimpleFunctionOnlyInc {
    param(
        [string] $Path,
        [string] $Text
    )

    if ($Text -match '(?m)^\s*#\s*(include|define|if|ifdef|ifndef|endif)') {
        return $false
    }
    if ($Text -match '(?m)^\s*(typedef|struct\s+[A-Za-z_]|enum\s+[A-Za-z_])') {
        return $false
    }
    if ($Text -notmatch '(?ms)^\s*static\s+[A-Za-z_][A-Za-z0-9_\s\*]*?\s+[A-Za-z_][A-Za-z0-9_]*\s*\([^;{}]*?\)\s*\{') {
        return $false
    }
    if ($Text -match '(?m)^\s*static\s+(?![A-Za-z_][A-Za-z0-9_\s\*]*?\s+[A-Za-z_][A-Za-z0-9_]*\s*\([^;{}]*?\)\s*\{)') {
        return $false
    }

    return $true
}

function Get-CandidateScore {
    param(
        [string] $RelativePath,
        [string] $Text,
        [int] $LineCount
    )

    $Score = $LineCount

    if ($RelativePath -match '/(paths?|date|parse|labels?|helpers?|query|lookup)\.inc$') {
        $Score -= 25
    }
    if ($RelativePath -match '^(src/core|src/military_service|src/team)/') {
        $Score -= 10
    }
    if ($RelativePath -match '^src/core/core_league_context_parts/') {
        $Score += 90
    }
    if ($RelativePath -match 'wic_bitmap\.inc$') {
        $Score += 300
    }
    if ($Text -match '\bg_[A-Za-z0-9_]+\b') {
        $Score += 80
    }
    if ($Text -match '\bKbo[A-Z][A-Za-z0-9_]*\b') {
        $Score += 45
    }
    if ($Text -match '\b(CreateThread|Interlocked|CRITICAL_SECTION|HANDLE|HWND|HDC|ICoreWebView2|CALLBACK|VirtualAlloc)\b') {
        $Score += 35
    }
    if ($RelativePath -match 'hook_stubs|patch_installers|patch_helpers|ui_|hotkey_window') {
        $Score += 35
    }
    if ($Text -match '\bappend_logf?\b') {
        $Score += 8
    }

    return $Score
}

$Candidates = @()
$StaticFunctionOwner = @{}
foreach ($File in Get-ChildItem -LiteralPath (Join-Path $NativeRoot "src") -Recurse -Filter "*.inc") {
    $Text = Get-Content -LiteralPath $File.FullName -Raw
    $RelativePath = Get-RelativeNativePath -Path $File.FullName
    foreach ($Match in [regex]::Matches($Text, '(?ms)^\s*static\s+[A-Za-z_][A-Za-z0-9_\s\*]*?\s+([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*?\)\s*\{')) {
        $StaticFunctionOwner[$Match.Groups[1].Value] = $RelativePath
    }
}

foreach ($File in Get-ChildItem -LiteralPath (Join-Path $NativeRoot "src") -Recurse -Filter "*.inc") {
    $Text = Get-Content -LiteralPath $File.FullName -Raw
    $RelativePath = Get-RelativeNativePath -Path $File.FullName
    if (-not (Test-SimpleFunctionOnlyInc -Path $File.FullName -Text $Text)) {
        continue
    }
    if (-not $IncludeStateful) {
        if ($Text -match '\bg_[A-Za-z0-9_]+\b') {
            continue
        }
        if ($Text -match '\bKbo[A-Z][A-Za-z0-9_]*\b') {
            continue
        }
    }
    if ($Text -match '\bfind_kbo_team_by_[A-Za-z0-9_]*\s*\(') {
        continue
    }
    if ($Text -match '\b(copy_ootp_string_object_text|get_kbo_league_event_manager)\s*\(') {
        continue
    }
    if ($Text -match '\b(find_kbo_global_player_vector|kbo_resolve_kbo_league_id|kbo_player_pointer_plausible)\s*\(') {
        continue
    }
    if ($Text -match '\b(kbo_json_skip_ws|kbo_json_find_string_end|kbo_json_string_equals_key|kbo_json_bool_value_at|kbo_json_int_value_at)\s*\(') {
        continue
    }

    $DependsOnSiblingStatic = $false
    foreach ($Match in [regex]::Matches($Text, '\b([A-Za-z_][A-Za-z0-9_]*)\s*\(')) {
        $Name = $Match.Groups[1].Value
        if ($StaticFunctionOwner.ContainsKey($Name) -and $StaticFunctionOwner[$Name] -ne $RelativePath) {
            $DependsOnSiblingStatic = $true
            break
        }
    }
    if ($DependsOnSiblingStatic) {
        continue
    }

    $LineCount = ($Text -split "`r?`n").Count
    $Candidates += [pscustomobject]@{
        Score = Get-CandidateScore -RelativePath $RelativePath -Text $Text -LineCount $LineCount
        Lines = $LineCount
        Path = $RelativePath
        FullName = $File.FullName
    }
}

$Selected = $Candidates |
    Sort-Object Score, Lines, Path |
    Select-Object -First $Limit

if ($Selected.Count -eq 0) {
    Write-Host "No automatic simple .inc candidates found."
    exit 0
}

Write-Host "Selected automatic .inc promotions:"
$Selected | Format-Table Score, Lines, Path -AutoSize

if ($WhatIf) {
    exit 0
}

& $PromoteScript -IncFiles ($Selected | ForEach-Object { $_.FullName })
