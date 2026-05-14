param(
    [string]$RepoRoot = "",
    [string]$Dist = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
}

if ([string]::IsNullOrWhiteSpace($Dist)) {
    $Dist = Join-Path $RepoRoot "dist"
}

if (-not (Test-Path -LiteralPath $Dist -PathType Container)) {
    throw "Release artifact directory does not exist: $Dist"
}

$requiredFiles = @(
    "KBOLauncher.exe",
    "KBOFix.dll",
    "WebView2Loader.dll",
    "kbo_league_id.txt",
    "assets\fonts\JejuGothic-Regular.ttf",
    "assets\fonts\JejuGothic-OFL.txt",
    "assets\icons\github-mark.png",
    "data\seeds\allstar_teams.csv",
    "data\seeds\asian_games_projected_hosts.csv",
    "data\seeds\asian_games_projected_policy.json",
    "data\seeds\asian_games_schedule_seed.csv",
    "data\seeds\captain_seed.csv",
    "data\seeds\cbt_player_team_seasons_seed.csv",
    "data\seeds\cbt_rules.json",
    "data\seeds\college_reputation_seed.csv",
    "data\seeds\economic_defaults.json",
    "data\seeds\fa_rules.json",
    "data\seeds\foreign_injury_replacements_seed.csv",
    "data\seeds\high_school_reputation_seed.csv",
    "data\seeds\kbo_team_policy.json",
    "data\seeds\military_service_seed.csv",
    "data\seeds\news_templates\en\asian_games.json",
    "data\seeds\news_templates\en\captain.json",
    "data\seeds\news_templates\en\competitive_balance_tax.json",
    "data\seeds\news_templates\en\custom_events.json",
    "data\seeds\news_templates\en\fa_compensation.json",
    "data\seeds\news_templates\en\foreign_injury.json",
    "data\seeds\news_templates\en\foreign_waiver.json",
    "data\seeds\news_templates\en\military_service.json",
    "data\seeds\news_templates\ko\asian_games.json",
    "data\seeds\news_templates\ko\captain.json",
    "data\seeds\news_templates\ko\competitive_balance_tax.json",
    "data\seeds\news_templates\ko\custom_events.json",
    "data\seeds\news_templates\ko\fa_compensation.json",
    "data\seeds\news_templates\ko\foreign_injury.json",
    "data\seeds\news_templates\ko\foreign_waiver.json",
    "data\seeds\news_templates\ko\military_service.json",
    "data\seeds\ui_text\en\hotkey_window.json",
    "data\seeds\ui_text\ko\hotkey_window.json",
    "tools\kbo_optimizer.exe",
    "tools\kbo_optimizer.py"
)

foreach ($requiredFile in $requiredFiles) {
    $path = Join-Path $Dist $requiredFile
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Release payload missing required file: $requiredFile"
    }
}

$manifest = Join-Path $RepoRoot "config\ootp-supported-builds.json"
$managedGenerated = Join-Path $RepoRoot "src\KBOLauncher\Infrastructure\OotpSupportedBuilds.Generated.cs"
$nativeGenerated = Join-Path $RepoRoot "native\src\build_verify\supported_builds.generated.c"
$nativeDll = Join-Path $RepoRoot "native\bin\KBOFix.dll"
$launcherExe = Join-Path $Dist "KBOLauncher.exe"

foreach ($path in @($manifest, $managedGenerated, $nativeGenerated, $nativeDll, $launcherExe)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Release validation input missing: $path"
    }
}

$manifestTime = (Get-Item -LiteralPath $manifest).LastWriteTimeUtc
foreach ($generated in @($managedGenerated, $nativeGenerated)) {
    if ((Get-Item -LiteralPath $generated).LastWriteTimeUtc -lt $manifestTime) {
        throw "Generated supported-build file is older than manifest: $generated"
    }
}

if ((Get-Item -LiteralPath $nativeDll).LastWriteTimeUtc -gt (Get-Item -LiteralPath $launcherExe).LastWriteTimeUtc) {
    throw "Native build appears newer than managed publish; run native build before publishing launcher."
}

$leagueId = (Get-Content -LiteralPath (Join-Path $Dist "kbo_league_id.txt") -Raw).Trim()
if ($leagueId -ne "100") {
    throw "Release payload has unexpected kbo_league_id.txt value: '$leagueId'"
}

$fileCount = (Get-ChildItem -LiteralPath $Dist -Recurse -File).Count
Write-Host "Release artifact verified: $fileCount files in $Dist"
