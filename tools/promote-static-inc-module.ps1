param(
    [Parameter(Mandatory = $true)]
    [string[]] $IncFiles
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$NativeRoot = Join-Path $RepoRoot "native"

function Get-RelativeNativePath {
    param([string] $Path)

    $FullPath = (Resolve-Path -LiteralPath $Path).Path
    $NativeFull = (Resolve-Path -LiteralPath $NativeRoot).Path
    if (-not $FullPath.StartsWith($NativeFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is not under native/: $Path"
    }

    return $FullPath.Substring($NativeFull.Length + 1).Replace("\", "/")
}

function New-HeaderGuard {
    param([string] $RelativeHeader)

    $Guard = $RelativeHeader.ToUpperInvariant() -replace '[^A-Z0-9]+', '_'
    return "KBOFIX_$($Guard)_"
}

function Write-Utf8NoBom {
    param(
        [string] $Path,
        [string] $Text
    )

    $Encoding = New-Object System.Text.UTF8Encoding($false)
    [IO.File]::WriteAllText($Path, $Text, $Encoding)
}

function Get-RelativeIncludePath {
    param(
        [string] $FromDirectory,
        [string] $ToPath
    )

    $FromUri = New-Object System.Uri(($FromDirectory.TrimEnd('\') + '\'))
    $ToUri = New-Object System.Uri($ToPath)
    return $FromUri.MakeRelativeUri($ToUri).ToString()
}

function Get-FunctionSignatures {
    param([string] $Text)

    $Pattern = '(?ms)^\s*static\s+([A-Za-z_][A-Za-z0-9_\s\*]*?\s+[A-Za-z_][A-Za-z0-9_]*\s*\([^;{}]*?\))\s*\{'
    $Matches = [regex]::Matches($Text, $Pattern)
    if ($Matches.Count -eq 0) {
        throw "No static function definitions found."
    }

    $Signatures = @()
    foreach ($Match in $Matches) {
        $Signature = ($Match.Groups[1].Value -replace '\s+', ' ').Trim()
        $Signature = $Signature -replace '\s+\*', '*'
        $Signature = $Signature -replace '\(\s+', '('
        $Signature = $Signature -replace '\s+\)', ')'
        $Signatures += $Signature
    }

    return $Signatures
}

function Get-CalledFunctionNames {
    param([string] $Text)

    $Names = New-Object System.Collections.Generic.HashSet[string]
    foreach ($Match in [regex]::Matches($Text, '\b([A-Za-z_][A-Za-z0-9_]*)\s*\(')) {
        $Name = $Match.Groups[1].Value
        if ($Name -in @("if", "for", "while", "switch", "return", "sizeof")) {
            continue
        }
        [void]$Names.Add($Name)
    }
    foreach ($Match in [regex]::Matches($Text, '\b(build_[A-Za-z_][A-Za-z0-9_]*)\b')) {
        [void]$Names.Add($Match.Groups[1].Value)
    }
    return $Names
}

function Get-HeadersForCalls {
    param(
        [string] $Text,
        [string] $SourceDir,
        [string] $HeaderPath
    )

    $CalledNames = Get-CalledFunctionNames -Text $Text
    if ($CalledNames.Count -eq 0) {
        return @()
    }

    $Includes = New-Object System.Collections.Generic.HashSet[string]
    foreach ($Header in Get-ChildItem -LiteralPath (Join-Path $NativeRoot "src") -Recurse -Filter "*.h") {
        if ($Header.FullName -eq $HeaderPath) {
            continue
        }
        $HeaderText = Get-Content -LiteralPath $Header.FullName -Raw
        foreach ($Name in $CalledNames) {
            if ($HeaderText -match "\b$([regex]::Escape($Name))\s*\(") {
                [void]$Includes.Add((Get-RelativeIncludePath -FromDirectory $SourceDir -ToPath $Header.FullName))
                break
            }
        }
    }

    return @($Includes | Sort-Object)
}

function Assert-SimpleInc {
    param(
        [string] $Path,
        [string] $Text
    )

    if ($Text -match '(?m)^\s*#\s*(include|define|if|ifdef|ifndef|endif)') {
        throw "Refusing to promote preprocessor-heavy file: $Path"
    }
    if ($Text -match '(?m)^\s*(typedef|struct\s+[A-Za-z_]|enum\s+[A-Za-z_])') {
        throw "Refusing to promote type-defining file: $Path"
    }
    if ($Text -match '(?m)^\s*static\s+(?![A-Za-z_][A-Za-z0-9_\s\*]*?\s+[A-Za-z_][A-Za-z0-9_]*\s*\([^;{}]*?\)\s*\{)') {
        throw "Refusing to promote file with static data or unsupported static declaration: $Path"
    }
}

foreach ($IncFile in $IncFiles) {
    $IncPath = (Resolve-Path -LiteralPath $IncFile).Path
    if ([IO.Path]::GetExtension($IncPath) -ne ".inc") {
        throw "Expected .inc file: $IncFile"
    }

    $Text = Get-Content -LiteralPath $IncPath -Raw
    Assert-SimpleInc -Path $IncPath -Text $Text
    $Signatures = Get-FunctionSignatures -Text $Text

    $BasePath = [IO.Path]::Combine([IO.Path]::GetDirectoryName($IncPath), [IO.Path]::GetFileNameWithoutExtension($IncPath))
    $SourcePath = "$BasePath.c"
    $HeaderPath = "$BasePath.h"
    if ((Test-Path -LiteralPath $SourcePath) -or (Test-Path -LiteralPath $HeaderPath)) {
        throw "Target .c/.h already exists for $IncFile"
    }

    $RelativeInc = Get-RelativeNativePath -Path $IncPath
    $RelativeSource = $RelativeInc -replace '\.inc$', '.c'
    $RelativeHeader = $RelativeInc -replace '\.inc$', '.h'
    $Guard = New-HeaderGuard -RelativeHeader $RelativeHeader

    $HeaderLines = @(
        "#ifndef $Guard",
        "#define $Guard",
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "#include <windows.h>",
        ""
    )
    foreach ($Signature in $Signatures) {
        $HeaderLines += "$Signature;"
    }
    $HeaderLines += @(
        "",
        "#endif"
    )

    $SourceDir = [IO.Path]::GetDirectoryName($SourcePath)
    $OffsetsInclude = Get-RelativeIncludePath -FromDirectory $SourceDir -ToPath (Join-Path $NativeRoot "src\bootstrap\ootp_offsets.h")
    $LogInclude = Get-RelativeIncludePath -FromDirectory $SourceDir -ToPath (Join-Path $NativeRoot "src\core\core_log.h")
    $CurrentDateInclude = Get-RelativeIncludePath -FromDirectory $SourceDir -ToPath (Join-Path $NativeRoot "src\core\core_current_date.h")
    $SavePathsInclude = Get-RelativeIncludePath -FromDirectory $SourceDir -ToPath (Join-Path $NativeRoot "src\core\core_save_paths.h")
    $TextDateInclude = Get-RelativeIncludePath -FromDirectory $SourceDir -ToPath (Join-Path $NativeRoot "src\core\core_text_date.h")
    $FlagsInclude = Get-RelativeIncludePath -FromDirectory $SourceDir -ToPath (Join-Path $NativeRoot "src\core\core_flags\flags_api.h")
    $RuntimeMemoryInclude = Get-RelativeIncludePath -FromDirectory $SourceDir -ToPath (Join-Path $NativeRoot "src\runtime_memory\runtime_memory.h")
    $PatchHelpersInclude = Get-RelativeIncludePath -FromDirectory $SourceDir -ToPath (Join-Path $NativeRoot "src\patch_helpers\patch_helpers.h")
    $ArbitrationPatchHelpersInclude = Get-RelativeIncludePath -FromDirectory $SourceDir -ToPath (Join-Path $NativeRoot "src\patch_installers\arbitration\arbitration_patch_helpers.h")
    $HookEntrypointsInclude = Get-RelativeIncludePath -FromDirectory $SourceDir -ToPath (Join-Path $NativeRoot "src\bootstrap\hook_entrypoints.h")
    $NearCodeInclude = Get-RelativeIncludePath -FromDirectory $SourceDir -ToPath (Join-Path $NativeRoot "src\hook_stubs\hook_stubs_near_code.h")
    $SourceText = $Text -replace '(?m)^(\s*)static\s+', '$1'
    $SourceLines = @(
        "#include `"$([IO.Path]::GetFileName($HeaderPath))`"",
        "#include <stdio.h>",
        "#include <string.h>",
        "#include `"$OffsetsInclude`"",
        "#include `"$LogInclude`"",
        "#include `"$CurrentDateInclude`"",
        "#include `"$SavePathsInclude`"",
        "#include `"$TextDateInclude`"",
        "#include `"$FlagsInclude`"",
        "#include `"$RuntimeMemoryInclude`""
    )
    if ($Text -match '\b(write_u32|write_u64|patch_static_bytes|resolve_patch_target_by_rva_or_[A-Za-z0-9_]+)\s*\(') {
        $SourceLines += "#include `"$PatchHelpersInclude`""
    }
    if ($Text -match '\ballocate_kbo_salary_arbitration_stub\s*\(') {
        $SourceLines += "#include `"$ArbitrationPatchHelpersInclude`""
    }
    if ($Text -match '&\s*ootp_kbo_[A-Za-z0-9_]+') {
        $SourceLines += "#include `"$HookEntrypointsInclude`""
    }
    if ($Text -match '\bkbo_alloc_near_code\s*\(') {
        $SourceLines += "#include `"$NearCodeInclude`""
    }
    foreach ($AutoHeaderInclude in Get-HeadersForCalls -Text $Text -SourceDir $SourceDir -HeaderPath $HeaderPath) {
        $Line = "#include `"$AutoHeaderInclude`""
        if ($SourceLines -notcontains $Line) {
            $SourceLines += $Line
        }
    }
    $SourceLines += ""
    $SourceLines += $SourceText
    $SourceText = $SourceLines -join "`r`n"

    $IncludePatterns = @(
        "#include `"$RelativeInc`"",
        "#include `"$([IO.Path]::GetFileName($IncPath))`""
    )
    $Parents = Get-ChildItem -LiteralPath $NativeRoot -Recurse -File -Include *.c,*.inc,*.h |
        Where-Object { $_.FullName -ne $IncPath } |
        Where-Object {
            $CandidateText = Get-Content -LiteralPath $_.FullName -Raw
            foreach ($Pattern in $IncludePatterns) {
                if ($CandidateText -like "*$Pattern*") {
                    return $true
                }
            }
            return $false
        }
    if ($Parents.Count -ne 1) {
        throw "Expected exactly one include parent for $RelativeInc, found $($Parents.Count)"
    }

    $ParentText = Get-Content -LiteralPath $Parents[0].FullName -Raw
    $ParentRelativeDir = Split-Path -Parent (Get-RelativeNativePath -Path $Parents[0].FullName)
    $HeaderForParent = if ($ParentRelativeDir -eq (Split-Path -Parent $RelativeHeader)) {
        [IO.Path]::GetFileName($HeaderPath)
    }
    else {
        $RelativeHeader
    }
    foreach ($Pattern in $IncludePatterns) {
        $ParentText = $ParentText.Replace($Pattern, "#include `"$HeaderForParent`"")
    }

    Write-Utf8NoBom -Path $HeaderPath -Text (($HeaderLines -join "`r`n").TrimEnd() + "`r`n")
    Write-Utf8NoBom -Path $SourcePath -Text ($SourceText.TrimEnd() + "`r`n")
    Write-Utf8NoBom -Path $Parents[0].FullName -Text ($ParentText.TrimEnd() + "`r`n")
    Remove-Item -LiteralPath $IncPath

    Write-Host "Promoted $RelativeInc -> $RelativeSource + $RelativeHeader"
}
