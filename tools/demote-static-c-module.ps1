param(
    [Parameter(Mandatory = $true)]
    [string[]] $SourceFiles
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

function Write-Utf8NoBom {
    param(
        [string] $Path,
        [string] $Text
    )

    $Encoding = New-Object System.Text.UTF8Encoding($false)
    [IO.File]::WriteAllText($Path, $Text, $Encoding)
}

foreach ($SourceFile in $SourceFiles) {
    $SourcePath = (Resolve-Path -LiteralPath $SourceFile).Path
    if ([IO.Path]::GetExtension($SourcePath) -ne ".c") {
        throw "Expected .c file: $SourceFile"
    }

    $BasePath = [IO.Path]::Combine([IO.Path]::GetDirectoryName($SourcePath), [IO.Path]::GetFileNameWithoutExtension($SourcePath))
    $HeaderPath = "$BasePath.h"
    $IncPath = "$BasePath.inc"
    if (-not (Test-Path -LiteralPath $HeaderPath)) {
        throw "Missing header beside $SourceFile"
    }

    $RelativeSource = Get-RelativeNativePath -Path $SourcePath
    $RelativeHeader = $RelativeSource -replace '\.c$', '.h'
    $RelativeInc = $RelativeSource -replace '\.c$', '.inc'

    $HeaderPatterns = @(
        "#include `"$RelativeHeader`"",
        "#include `"$([IO.Path]::GetFileName($HeaderPath))`""
    )
    $Parents = Get-ChildItem -LiteralPath $NativeRoot -Recurse -File -Include *.c,*.inc,*.h |
        Where-Object { $_.FullName -ne $SourcePath -and $_.FullName -ne $HeaderPath } |
        Where-Object {
            $CandidateText = Get-Content -LiteralPath $_.FullName -Raw
            foreach ($Pattern in $HeaderPatterns) {
                if ($CandidateText -like "*$Pattern*") {
                    return $true
                }
            }
            return $false
        }
    if ($Parents.Count -ne 1) {
        throw "Expected exactly one include parent for $RelativeHeader, found $($Parents.Count)"
    }

    $SourceText = Get-Content -LiteralPath $SourcePath -Raw
    $IncText = $SourceText -replace '(?m)^#include\s+"[^"]+"\r?\n\r?\n?', ''
    $IncText = $IncText -replace '(?m)^([A-Za-z_][A-Za-z0-9_\s\*]*?\s+[A-Za-z_][A-Za-z0-9_]*\s*\([^;{}]*?\)\s*\{)', 'static $1'

    $ParentText = Get-Content -LiteralPath $Parents[0].FullName -Raw
    $ParentRelativeDir = Split-Path -Parent (Get-RelativeNativePath -Path $Parents[0].FullName)
    $IncForParent = if ($ParentRelativeDir -eq (Split-Path -Parent $RelativeInc)) {
        [IO.Path]::GetFileName($IncPath)
    }
    else {
        $RelativeInc
    }
    foreach ($Pattern in $HeaderPatterns) {
        $ParentText = $ParentText.Replace($Pattern, "#include `"$IncForParent`"")
    }

    Write-Utf8NoBom -Path $IncPath -Text ($IncText.TrimEnd() + "`r`n")
    Write-Utf8NoBom -Path $Parents[0].FullName -Text ($ParentText.TrimEnd() + "`r`n")
    Remove-Item -LiteralPath $SourcePath
    Remove-Item -LiteralPath $HeaderPath

    Write-Host "Demoted $RelativeSource -> $RelativeInc"
}
