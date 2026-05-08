param(
    [int] $SampleLimit = 25
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$NativeSrc = Join-Path $RepoRoot "native\src"
$NativeSrcFull = (Resolve-Path -LiteralPath $NativeSrc).Path

function Get-RelativeSrcPath {
    param([string] $Path)

    return $Path.Substring($NativeSrcFull.Length + 1).Replace("\", "/")
}

function Get-IncFacts {
    param(
        [string] $Path,
        [hashtable] $StaticFunctionOwner
    )

    $Text = Get-Content -LiteralPath $Path -Raw
    $RelativePath = Get-RelativeSrcPath -Path $Path
    $FunctionDefinitions = [regex]::Matches(
        $Text,
        '(?ms)^\s*static\s+[A-Za-z_][A-Za-z0-9_\s\*]*?\s+[A-Za-z_][A-Za-z0-9_]*\s*\([^;{}]*?\)\s*\{'
    ).Count
    $NonStaticFunctionDefinitions = [regex]::Matches(
        $Text,
        '(?ms)^\s*(?:__declspec\([^\)]*\)\s*)?(?!static\b)[A-Za-z_][A-Za-z0-9_\s\*]*?\s+[A-Za-z_][A-Za-z0-9_]*\s*\([^;{}]*?\)\s*\{'
    ).Count
    $HasPreprocessor = [regex]::IsMatch($Text, '(?m)^\s*#\s*(include|define|if|ifdef|ifndef|endif|pragma)')
    $HasTypeDefinition = [regex]::IsMatch($Text, '(?m)^\s*(typedef|struct\s+[A-Za-z_]|enum\s+[A-Za-z_])')
    $HasStaticData = [regex]::IsMatch(
        $Text,
        '(?m)^\s*static\s+(?![A-Za-z_][A-Za-z0-9_\s\*]*?\s+[A-Za-z_][A-Za-z0-9_]*\s*\([^;{}]*?\)\s*\{)'
    )
    $HasGlobalData = [regex]::IsMatch(
        $Text,
        '(?m)^\s*(?!static\b)(?:const\s+)?(?:uint\d+_t|int\d+_t|int|char|size_t|uintptr_t|HANDLE|HWND|RECT|CRITICAL_SECTION|volatile|LONG|DWORD)\s+[A-Za-z_][A-Za-z0-9_]*(\s*\[[^\]]*\])?\s*(=|;)'
    )

    $OnlyStaticFunctions = $FunctionDefinitions -gt 0 -and -not $HasPreprocessor -and -not $HasTypeDefinition -and -not $HasStaticData
    $Reasons = New-Object System.Collections.Generic.List[string]
    if ($OnlyStaticFunctions) {
        if ($Text -match '\bg_[A-Za-z0-9_]+\b') {
            $Reasons.Add("g_state")
        }
        if ($Text -match '\bKbo[A-Z][A-Za-z0-9_]*\b') {
            $Reasons.Add("Kbo_type")
        }
        if ($Text -match '\b(CreateThread|Interlocked|CRITICAL_SECTION|HANDLE|HWND|HDC|ICoreWebView2|CALLBACK|VirtualAlloc)\b') {
            $Reasons.Add("win_thread_ui_mem")
        }
        if ($Text -match '\bfind_kbo_team_by_[A-Za-z0-9_]*\s*\(') {
            $Reasons.Add("team_lookup")
        }
        if ($Text -match '\b(copy_ootp_string_object_text|get_kbo_league_event_manager)\s*\(') {
            $Reasons.Add("ootp_string_or_event_mgr")
        }
        if ($Text -match '\b(find_kbo_global_player_vector|kbo_resolve_kbo_league_id|kbo_player_pointer_plausible)\s*\(') {
            $Reasons.Add("league_or_player_core")
        }
        if ($Text -match '\b(kbo_json_skip_ws|kbo_json_find_string_end|kbo_json_string_equals_key|kbo_json_bool_value_at|kbo_json_int_value_at)\s*\(') {
            $Reasons.Add("json_helpers")
        }
    }

    $SiblingReferences = New-Object System.Collections.Generic.List[string]
    foreach ($Match in [regex]::Matches($Text, '\b([A-Za-z_][A-Za-z0-9_]*)\s*\(')) {
        $Name = $Match.Groups[1].Value
        if ($StaticFunctionOwner.ContainsKey($Name) -and $StaticFunctionOwner[$Name] -ne $RelativePath) {
            $SiblingReferences.Add("$($StaticFunctionOwner[$Name]):$Name")
        }
    }
    if ($SiblingReferences.Count -gt 0) {
        $Reasons.Add("sibling_static")
    }
    if ($OnlyStaticFunctions -and $Reasons.Count -eq 0) {
        $Reasons.Add("unknown_auto_gap")
    }

    $PrimaryBlocker = if ($OnlyStaticFunctions) {
        "only-static-funcs"
    }
    elseif ($HasPreprocessor) {
        "preprocessor"
    }
    elseif ($HasTypeDefinition) {
        "types"
    }
    elseif ($HasStaticData) {
        "static-data"
    }
    elseif ($HasGlobalData) {
        "global-data"
    }
    elseif ($NonStaticFunctionDefinitions -gt 0) {
        "nonstatic-funcs"
    }
    else {
        "other"
    }

    [pscustomobject]@{
        Path = $RelativePath
        Lines = ($Text -split "`r?`n").Count
        PrimaryBlocker = $PrimaryBlocker
        FunctionDefinitions = $FunctionDefinitions
        NonStaticFunctionDefinitions = $NonStaticFunctionDefinitions
        HasPreprocessor = $HasPreprocessor
        HasInclude = $Text -match '(?m)^\s*#\s*include'
        HasDefine = $Text -match '(?m)^\s*#\s*define'
        HasTypeDefinition = $HasTypeDefinition
        HasStaticData = $HasStaticData
        HasGlobalData = $HasGlobalData
        OnlyStaticFunctions = $OnlyStaticFunctions
        Reasons = (($Reasons | Select-Object -Unique) -join ",")
        SiblingReferences = (($SiblingReferences | Select-Object -Unique) -join ";")
    }
}

$IncFiles = Get-ChildItem -LiteralPath $NativeSrc -Recurse -Filter "*.inc"
$StaticFunctionOwner = @{}
foreach ($File in $IncFiles) {
    $Text = Get-Content -LiteralPath $File.FullName -Raw
    $RelativePath = Get-RelativeSrcPath -Path $File.FullName
    foreach ($Match in [regex]::Matches($Text, '(?ms)^\s*static\s+[A-Za-z_][A-Za-z0-9_\s\*]*?\s+([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*?\)\s*\{')) {
        $StaticFunctionOwner[$Match.Groups[1].Value] = $RelativePath
    }
}

$Facts = foreach ($File in $IncFiles) {
    Get-IncFacts -Path $File.FullName -StaticFunctionOwner $StaticFunctionOwner
}

Write-Host "Remaining .inc files: $($Facts.Count)"
Write-Host ""
Write-Host "Primary blockers:"
$Facts |
    Group-Object PrimaryBlocker |
    Sort-Object Count -Descending |
    Format-Table Count, Name -AutoSize

Write-Host ""
Write-Host "Feature counts:"
[pscustomobject]@{
    Total = $Facts.Count
    OnlyStaticFunctions = ($Facts | Where-Object OnlyStaticFunctions).Count
    Preprocessor = ($Facts | Where-Object HasPreprocessor).Count
    TypeDefinitions = ($Facts | Where-Object HasTypeDefinition).Count
    StaticData = ($Facts | Where-Object HasStaticData).Count
    GlobalData = ($Facts | Where-Object HasGlobalData).Count
    HasInclude = ($Facts | Where-Object HasInclude).Count
    HasDefine = ($Facts | Where-Object HasDefine).Count
} | Format-List

Write-Host ""
Write-Host "Only-static function blockers:"
$Facts |
    Where-Object OnlyStaticFunctions |
    ForEach-Object { $_.Reasons -split "," } |
    Where-Object { $_ } |
    Group-Object |
    Sort-Object Count -Descending |
    Format-Table Count, Name -AutoSize

Write-Host ""
Write-Host "Small preprocessor function candidates:"
$Facts |
    Where-Object { $_.HasPreprocessor -and $_.FunctionDefinitions -gt 0 -and -not $_.HasTypeDefinition -and -not $_.HasStaticData } |
    Sort-Object Lines |
    Select-Object -First $SampleLimit Path, Lines, FunctionDefinitions, HasInclude, HasDefine |
    Format-Table -AutoSize

Write-Host ""
Write-Host "Small state/data candidates:"
$Facts |
    Where-Object { $_.HasStaticData -and -not $_.HasTypeDefinition } |
    Sort-Object Lines |
    Select-Object -First $SampleLimit Path, Lines, FunctionDefinitions, HasPreprocessor |
    Format-Table -AutoSize

Write-Host ""
Write-Host "Small sibling-static clusters:"
$Facts |
    Where-Object { $_.OnlyStaticFunctions -and $_.Reasons -eq "sibling_static" } |
    Sort-Object Lines |
    Select-Object -First $SampleLimit Path, Lines, SiblingReferences |
    Format-Table -AutoSize
