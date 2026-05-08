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
    $SourceText = $Text -replace '(?m)^(\s*)static\s+', '$1'
    $SourceText = @(
        "#include `"$([IO.Path]::GetFileName($HeaderPath))`"",
        "#include <stdio.h>",
        "#include <string.h>",
        "#include `"$OffsetsInclude`"",
        "#include `"$LogInclude`"",
        "#include `"$CurrentDateInclude`"",
        "#include `"$SavePathsInclude`"",
        "#include `"$TextDateInclude`"",
        "#include `"$FlagsInclude`"",
        "#include `"$RuntimeMemoryInclude`"",
        "",
        $SourceText
    ) -join "`r`n"

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
