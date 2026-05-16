#Requires -Version 7.0
param(
    [string] $PerfPath = "",
    [string] $BenchPath = "",
    [string] $MapPath = "",
    [string] $OutputPath = "",
    [string] $WriteReplayPlan = "",
    [switch] $PlanOnly,
    [double] $BaselineSeconds = -1,
    [double] $ModSeconds = -1,
    [double] $LastMinutes = -1,
    [int] $Top = 25,
    [int] $UnmappedTop = 25,
    [int] $GroupDepth = 1
)

$ErrorActionPreference = "Stop"

$Script = Join-Path $PSScriptRoot "analyze_perf_overhead.py"
$Python = Get-Command python -ErrorAction SilentlyContinue
if (-not $Python) {
    throw "python was not found on PATH"
}

$Args = @($Script, "--top", "$Top", "--unmapped-top", "$UnmappedTop", "--group-depth", "$GroupDepth")
if (-not [string]::IsNullOrWhiteSpace($PerfPath)) {
    $Args += @("--perf", $PerfPath)
}
if (-not [string]::IsNullOrWhiteSpace($BenchPath)) {
    $Args += @("--bench", $BenchPath)
}
if (-not [string]::IsNullOrWhiteSpace($MapPath)) {
    $Args += @("--map", $MapPath)
}
if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
    $Args += @("--output", $OutputPath)
}
if (-not [string]::IsNullOrWhiteSpace($WriteReplayPlan)) {
    $Args += @("--write-replay-plan", $WriteReplayPlan)
}
if ($PlanOnly) {
    $Args += @("--plan-only")
}
if ($BaselineSeconds -ge 0) {
    $Args += @("--baseline-seconds", "$BaselineSeconds")
}
if ($ModSeconds -ge 0) {
    $Args += @("--mod-seconds", "$ModSeconds")
}
if ($LastMinutes -gt 0) {
    $Args += @("--last-minutes", "$LastMinutes")
}

& $Python.Source @Args
if ($LASTEXITCODE -ne 0) {
    throw "perf overhead analysis failed"
}
