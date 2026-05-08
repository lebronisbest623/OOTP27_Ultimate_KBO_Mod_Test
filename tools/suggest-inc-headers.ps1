param(
    [int] $SampleLimit = 40
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$NativeSrc = Join-Path $RepoRoot "native\src"
$NativeSrcFull = (Resolve-Path -LiteralPath $NativeSrc).Path

function Get-RelativeSrcPath {
    param([string] $Path)

    return $Path.Substring($NativeSrcFull.Length + 1).Replace("\", "/")
}

function Get-StaticFunctionDefinitions {
    param(
        [string] $Path,
        [string] $Text
    )

    $RelativePath = Get-RelativeSrcPath -Path $Path
    foreach ($Match in [regex]::Matches($Text, '(?ms)^\s*static\s+([A-Za-z_][A-Za-z0-9_\s\*]*?\s+([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*?\))\s*\{')) {
        [pscustomobject]@{
            Name = $Match.Groups[2].Value
            Signature = (($Match.Groups[1].Value -replace '\s+', ' ').Trim() -replace '\s+\*', '*')
            Owner = $RelativePath
            OwnerFullName = $Path
        }
    }
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
    return $Names
}

function Test-HeaderExistsForOwner {
    param([string] $OwnerFullName)

    $BasePath = [IO.Path]::Combine([IO.Path]::GetDirectoryName($OwnerFullName), [IO.Path]::GetFileNameWithoutExtension($OwnerFullName))
    return Test-Path -LiteralPath "$BasePath.h"
}

$IncFiles = Get-ChildItem -LiteralPath $NativeSrc -Recurse -Filter "*.inc"
$DefinitionsByName = @{}
$Definitions = New-Object System.Collections.Generic.List[object]

foreach ($File in $IncFiles) {
    $Text = Get-Content -LiteralPath $File.FullName -Raw
    foreach ($Definition in Get-StaticFunctionDefinitions -Path $File.FullName -Text $Text) {
        $Definitions.Add($Definition)
        if (-not $DefinitionsByName.ContainsKey($Definition.Name)) {
            $DefinitionsByName[$Definition.Name] = New-Object System.Collections.Generic.List[object]
        }
        $DefinitionsByName[$Definition.Name].Add($Definition)
    }
}

$ExternalUsesByOwner = @{}
foreach ($File in $IncFiles) {
    $Text = Get-Content -LiteralPath $File.FullName -Raw
    $Caller = Get-RelativeSrcPath -Path $File.FullName
    $CalledNames = Get-CalledFunctionNames -Text $Text

    foreach ($Name in $CalledNames) {
        if (-not $DefinitionsByName.ContainsKey($Name)) {
            continue
        }
        foreach ($Definition in $DefinitionsByName[$Name]) {
            if ($Definition.Owner -eq $Caller) {
                continue
            }
            if (-not $ExternalUsesByOwner.ContainsKey($Definition.Owner)) {
                $ExternalUsesByOwner[$Definition.Owner] = New-Object System.Collections.Generic.List[object]
            }
            $ExternalUsesByOwner[$Definition.Owner].Add([pscustomobject]@{
                Function = $Name
                Signature = $Definition.Signature
                UsedBy = $Caller
                OwnerFullName = $Definition.OwnerFullName
            })
        }
    }
}

$Rows = foreach ($Owner in $ExternalUsesByOwner.Keys) {
    $Uses = $ExternalUsesByOwner[$Owner]
    $UniqueFunctions = @($Uses | Select-Object Function, Signature -Unique)
    $UniqueCallers = @($Uses | Select-Object -ExpandProperty UsedBy -Unique)
    $OwnerFullName = $Uses[0].OwnerFullName
    [pscustomobject]@{
        Owner = $Owner
        Lines = ((Get-Content -LiteralPath $OwnerFullName -Raw) -split "`r?`n").Count
        ExternalFunctionCount = $UniqueFunctions.Count
        ExternalCallerCount = $UniqueCallers.Count
        HasSiblingHeader = Test-HeaderExistsForOwner -OwnerFullName $OwnerFullName
        Functions = (($UniqueFunctions | Select-Object -ExpandProperty Function) -join ",")
        Callers = (($UniqueCallers | Select-Object -First 4) -join ";")
    }
}

Write-Host "Header-first candidates from static cross-file usage:"
$Rows |
    Sort-Object HasSiblingHeader, ExternalCallerCount, ExternalFunctionCount, Lines -Descending |
    Select-Object -First $SampleLimit |
    Format-Table -AutoSize

Write-Host ""
Write-Host "Smallest missing-header candidates:"
$Rows |
    Where-Object { -not $_.HasSiblingHeader } |
    Sort-Object Lines, ExternalCallerCount -Descending |
    Select-Object -First $SampleLimit |
    Format-Table -AutoSize
