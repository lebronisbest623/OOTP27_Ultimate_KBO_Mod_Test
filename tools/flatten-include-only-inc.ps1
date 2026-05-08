param(
    [Parameter(Mandatory = $true)]
    [string]$TargetFile,

    [Parameter(Mandatory = $true)]
    [string[]]$IncFiles,

    [switch]$AllowForwardDeclarations
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-RepoRelativePath([string]$Path) {
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $repo = (Resolve-Path -LiteralPath ".").Path
    if (-not $resolved.StartsWith($repo, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside repository: $Path"
    }
    return $resolved.Substring($repo.Length + 1).Replace("\", "/")
}

function Get-RelativeIncludePath([string]$FromDirectory, [string]$ToPath) {
    $from = (Resolve-Path -LiteralPath $FromDirectory).Path
    $to = (Resolve-Path -LiteralPath $ToPath).Path
    $fromUri = [Uri]($from.TrimEnd("\") + "\")
    $toUri = [Uri]$to
    return [Uri]::UnescapeDataString($fromUri.MakeRelativeUri($toUri).ToString()).Replace("/", "/")
}

function Get-IncludeOnlyExpansion([string]$IncFile) {
    $lines = Get-Content -LiteralPath $IncFile
    $children = @()
    $prefix = @()
    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        if ($trimmed.Length -eq 0 -or $trimmed.StartsWith("/*") -or $trimmed.StartsWith("*") -or $trimmed.StartsWith("//")) {
            continue
        }
        if ($trimmed -match '^#include\s+"([^"]+)"\s*$') {
            $children += $Matches[1]
            continue
        }
        if ($AllowForwardDeclarations) {
            $prefix += $line
            continue
        }
        throw "Not include-only: $IncFile contains '$line'"
    }
    if ($children.Count -eq 0) {
        throw "No child includes found: $IncFile"
    }
    return [pscustomobject]@{ Prefix = $prefix; Children = $children }
}

function Assert-ForwardDeclarationBlock([string[]]$Prefix, [string]$IncFile) {
    if (-not $AllowForwardDeclarations -or $Prefix.Count -eq 0) {
        return
    }
    $joined = ($Prefix -join "`n").Trim()
    $statements = @($joined -split ';' | Where-Object { $_.Trim() -ne '' })
    foreach ($statement in $statements) {
        if (($statement.Trim() + ';') -notmatch '^static\s+[^;{}]+\([^{}]*\)\s*;$') {
            throw "Not include-only: $IncFile contains non-forward declaration '$($statement.Trim())'"
        }
    }
}

$targetPath = (Resolve-Path -LiteralPath $TargetFile).Path
$targetDir = Split-Path -Parent $targetPath
$targetLines = [System.Collections.Generic.List[string]]::new()
(Get-Content -LiteralPath $targetPath) | ForEach-Object { [void]$targetLines.Add($_) }

foreach ($inc in $IncFiles) {
    $incPath = (Resolve-Path -LiteralPath $inc).Path
    $incRepoRel = Get-RepoRelativePath $incPath
    $targetIncludeRel = Get-RelativeIncludePath $targetDir $incPath
    $includeLine = "#include `"$targetIncludeRel`""
    $matches = @()
    for ($i = 0; $i -lt $targetLines.Count; $i++) {
        if ($targetLines[$i].Trim() -eq $includeLine) {
            $matches += $i
        }
    }
    if ($matches.Count -ne 1) {
        throw "Expected exactly one target include for $incRepoRel, found $($matches.Count)"
    }

    $incDir = Split-Path -Parent $incPath
    $expansion = Get-IncludeOnlyExpansion $incPath
    Assert-ForwardDeclarationBlock $expansion.Prefix $incPath
    $childLines = @()
    $childLines += $expansion.Prefix
    foreach ($child in $expansion.Children) {
        $childPath = Join-Path $incDir $child
        if (-not (Test-Path -LiteralPath $childPath)) {
            throw "Child include does not exist: $childPath"
        }
        $childTargetRel = Get-RelativeIncludePath $targetDir $childPath
        $childLines += "#include `"$childTargetRel`""
    }

    $index = $matches[0]
    $targetLines.RemoveAt($index)
    for ($j = $childLines.Count - 1; $j -ge 0; $j--) {
        $targetLines.Insert($index, $childLines[$j])
    }
    Remove-Item -LiteralPath $incPath
    Write-Host "Flattened $incRepoRel -> $($childLines.Count) includes"
}

Set-Content -LiteralPath $targetPath -Value $targetLines
